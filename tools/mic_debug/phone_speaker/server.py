#!/usr/bin/env python3
"""LAN speaker page; playback commands are supplied through a local file, not HTTP."""

import argparse
import json
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

LOCK = threading.Lock()


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('root', type=Path)
  parser.add_argument('--bind', default='192.168.63.47')
  parser.add_argument('--port', type=int, default=8767)
  args = parser.parse_args()
  root = args.root.resolve()
  (root / 'clients').mkdir(exist_ok=True)

  class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
      pass

    def reply(self, data, content_type='application/json', status=200, headers=None):
      self.send_response(status)
      self.send_header('Content-Type', content_type)
      self.send_header('Cache-Control', 'no-store')
      self.send_header('Content-Length', str(len(data)))
      for key, value in (headers or {}).items():
        self.send_header(key, value)
      self.end_headers()
      self.wfile.write(data)

    def do_GET(self):
      path = urlparse(self.path).path
      if path == '/state':
        command = {'id': 0, 'action': 'idle'}
        try:
          command = json.loads((root / 'command.json').read_text())
        except (FileNotFoundError, json.JSONDecodeError):
          pass
        self.reply(json.dumps(command).encode())
      elif path in ('/', '/index.html', '/speaker.js', '/awake.mp4') or re.fullmatch(r'/phone-[a-z0-9-]+\.wav', path):
        name = 'index.html' if path == '/' else path[1:]
        if not (root / name).is_file():
          self.reply(b'{}', status=404)
          return
        content_type = (
          'audio/wav'
          if name.endswith('.wav')
          else {'index.html': 'text/html; charset=utf-8', 'speaker.js': 'text/javascript', 'phone-sweep.wav': 'audio/wav', 'awake.mp4': 'video/mp4'}[name]
        )
        data = (root / name).read_bytes()
        requested = self.headers.get('Range')
        if requested:
          match = re.fullmatch(r'bytes=(\d+)-(\d*)', requested)
          if not match:
            self.reply(b'', status=416)
            return
          start = int(match[1])
          end = min(int(match[2]) if match[2] else len(data) - 1, len(data) - 1)
          if start > end:
            self.reply(b'', status=416)
            return
          self.reply(data[start : end + 1], content_type, 206, {'Content-Range': f'bytes {start}-{end}/{len(data)}', 'Accept-Ranges': 'bytes'})
        else:
          self.reply(data, content_type, headers={'Accept-Ranges': 'bytes'})
      else:
        self.reply(b'{}', status=404)

    def do_POST(self):
      origin = self.headers.get('Origin')
      if origin and origin != 'http://' + self.headers.get('Host', ''):
        self.reply(b'{}', status=403)
        return
      length = int(self.headers.get('Content-Length', '0'))
      if urlparse(self.path).path != '/event' or not 0 < length < 16384 or self.headers.get('Content-Type') != 'application/json':
        self.reply(b'{}', status=400)
        return
      try:
        event = json.loads(self.rfile.read(length))
        client = event.get('client', '')
        if not re.fullmatch(r'[a-f0-9]{16,64}', client):
          raise ValueError('Invalid client')
        event['received_monotonic'] = time.monotonic()
        with LOCK:
          destination = root / 'clients' / (client + '.json')
          temporary = destination.with_suffix('.new')
          temporary.write_text(json.dumps(event))
          temporary.replace(destination)
          if event.get('event') != 'heartbeat':
            with (root / 'events.jsonl').open('a') as log:
              log.write(json.dumps(event) + '\n')
        self.reply(b'{"ok":true}')
      except (ValueError, TypeError, AttributeError):
        self.reply(b'{}', status=400)

  ThreadingHTTPServer((args.bind, args.port), Handler).serve_forever()


if __name__ == '__main__':
  main()
