// http_body.h - framing-aware, buffered reader for an HTTP response body.
//
// Wraps HTTPClient's raw socket so a JSON parse can never be silently truncated:
//   - When Content-Length is known, EOF happens exactly at the last body byte, so the
//     parse never depends on the peer closing the connection.
//   - When it is unknown (OpenSky /tracks sends no Content-Length), reading continues
//     while the peer is connected or has buffered data, and a stall longer than
//     stallMs is recorded as an error instead of looking like end-of-input.
//   - Reads are refilled in bulk. ArduinoJson asks for one character at a time, and each
//     one-byte read on NetworkClientSecure is a full mbedtls_ssl_read() round trip; the
//     buffer turns ~100k of those into ~100 for a 50 KB body.
//   - dechunk=true handles Transfer-Encoding: chunked, so callers are free to use
//     HTTP/1.1 keep-alive instead of forcing HTTP/1.0.
// After the parse, callers check complete() and log short()/stalled() rather than
// treating a truncated body as an empty result.
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class HttpBodyStream : public Stream {
 public:
  HttpBodyStream(HTTPClient& http, bool dechunk = false, uint32_t stallMs = 8000)
      : client_(http.getStreamPtr()),
        contentLength_(http.getSize()),
        remaining_(http.getSize()),
        dechunk_(dechunk),
        stallMs_(stallMs) {}

  // ---- Stream ----
  int available() override { return (int)(len_ - pos_); }
  int read() override {
    if (pos_ >= len_ && !refill()) return -1;
    return (unsigned char)buf_[pos_++];
  }
  int peek() override {
    if (pos_ >= len_ && !refill()) return -1;
    return (unsigned char)buf_[pos_];
  }
  void unread() { if (pos_ > 0) pos_--; }  // put the last read() byte back

  size_t readBytes(char* dst, size_t n) override {
    size_t got = 0;
    while (got < n) {
      if (pos_ >= len_ && !refill()) break;
      size_t take = n - got;
      size_t inBuf = len_ - pos_;
      if (take > inBuf) take = inBuf;
      memcpy(dst + got, buf_ + pos_, take);
      pos_ += take;
      got += take;
    }
    return got;
  }
  size_t write(uint8_t) override { return 0; }  // read-only
  void flush() override {}

  // ---- framing status (check these after the parse) ----
  size_t bytesRead() const { return total_; }
  long   contentLength() const { return contentLength_; }
  bool   stalled() const { return stalled_; }
  // True when we consumed exactly Content-Length bytes, or the peer closed cleanly on a
  // length-less body without stalling.
  bool   complete() const {
    if (contentLength_ > 0) return total_ == (size_t)contentLength_;
    return eof_ && !stalled_;
  }
  bool   shortRead() const { return contentLength_ > 0 && total_ < (size_t)contentLength_; }

  // Consume the rest of the response so complete() reflects whether the whole
  // body arrived. Use this after a parse that intentionally stops early
  // (e.g. once it has the one array it needs) but still wants a framing check.
  bool drain() {
    char tmp[512];
    while (readBytes(tmp, sizeof(tmp)) > 0) {}
    return complete();
  }

 private:
  bool refill() {
    pos_ = len_ = 0;
    if (eof_ || !client_) return false;
    if (remaining_ == 0) { eof_ = true; return false; }

    size_t want = sizeof buf_;
    if (dechunk_) {
      if (chunkLeft_ == 0 && !nextChunk()) return false;
      if (chunkLeft_ < want) want = chunkLeft_;
    } else if (remaining_ > 0) {
      if ((size_t)remaining_ < want) want = (size_t)remaining_;
    }

    uint32_t t0 = millis();
    for (;;) {
      int n = client_->available();
      if (n > 0) {
        size_t toRead = want;
        if ((size_t)n < toRead) toRead = (size_t)n;
        size_t got = client_->readBytes(buf_, toRead);
        if (got > 0) {
          len_ = got;
          total_ += got;
          if (remaining_ > 0) remaining_ -= (long)got;
          if (dechunk_) chunkLeft_ -= got;
          return true;
        }
      }
      // No data right now: distinguish "finished" from "stalled".
      if (!client_->connected() && client_->available() == 0) { eof_ = true; return false; }
      if (millis() - t0 > stallMs_) { stalled_ = true; eof_ = true; return false; }
      delay(1);
    }
  }

  // Read "<hex>\r\n" and set chunkLeft_. Returns false at the 0-length terminator chunk.
  bool nextChunk() {
    if (!skipCrLf()) return false;
    size_t size = 0;
    int digits = 0;
    for (;;) {
      int c = rawByte();
      if (c < 0) { stalled_ = true; eof_ = true; return false; }
      if (c == '\r') { rawByte(); break; }  // consume '\n'
      if (c == ';') { while (true) { int d = rawByte(); if (d < 0 || d == '\n') break; } break; }
      int v = (c >= '0' && c <= '9') ? c - '0'
            : (c >= 'a' && c <= 'f') ? c - 'a' + 10
            : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
      if (v < 0) { stalled_ = true; eof_ = true; return false; }
      size = size * 16 + (size_t)v;
      digits++;
    }
    if (!digits) { eof_ = true; return false; }
    if (size == 0) { eof_ = true; return false; }
    chunkLeft_ = size;
    return true;
  }

  bool skipCrLf() {
    if (!sawChunk_) { sawChunk_ = true; return true; }
    int c = rawByte();
    if (c == '\r') rawByte();
    return c >= 0;
  }

  int rawByte() {
    uint32_t t0 = millis();
    while (client_->available() == 0) {
      if (!client_->connected()) return -1;
      if (millis() - t0 > stallMs_) return -1;
      delay(1);
    }
    return client_->read();
  }

  NetworkClient* client_;
  long   contentLength_;
  long   remaining_;
  bool   dechunk_;
  uint32_t stallMs_;
  char   buf_[512];
  size_t pos_ = 0, len_ = 0, total_ = 0, chunkLeft_ = 0;
  bool   eof_ = false, stalled_ = false, sawChunk_ = false;
};

// Advance `s` past the next occurrence of `key` and its opening '['. Returns false if the
// key never appears. Used to stream-parse a large array nested in a small object, so the
// whole response never has to fit in a JsonDocument.
inline bool seekArray(HttpBodyStream& s, const char* key) {
  size_t k = 0, n = strlen(key);
  for (;;) {
    int c = s.read();
    if (c < 0) return false;
    k = (c == (unsigned char)key[k]) ? k + 1 : (c == (unsigned char)key[0] ? 1 : 0);
    if (k == n) break;
  }
  for (;;) {  // skip ':' / whitespace up to the value
    int c = s.read();
    if (c < 0) return false;
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
    if (c == ':') continue;
    if (c == '[') return true;
    // Not an array (e.g. "states":null or "path":null). Put it back on the
    // stream by leaving it unread (the value starts at the current position).
    s.unread();
    return false;
  }
}

// Read one element of the array we are positioned inside. Returns false at ']' or on error.
inline bool nextElement(HttpBodyStream& s, JsonDocument& doc) {
  for (;;) {  // skip ',' / whitespace; stop at ']'
    int c = s.peek();
    if (c < 0) return false;
    if (c == ']') { s.read(); return false; }
    if (c == ',' || c == ' ' || c == '\r' || c == '\n' || c == '\t') { s.read(); continue; }
    break;
  }
  DeserializationError err = deserializeJson(doc, s);
  return err == DeserializationError::Ok;
}
