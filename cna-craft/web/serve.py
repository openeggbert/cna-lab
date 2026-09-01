#!/usr/bin/env python3
"""Static file server for the cna-craft web build, with the COOP/COEP headers
wasm threads need (plan.md §12.1 item 37).

The threaded wasm build stores its heap in a SharedArrayBuffer, which browsers
only expose to *cross-origin isolated* pages -- meaning the server must send:

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

`python3 -m http.server` sends neither, so the page loads without
SharedArrayBuffer and the runtime refuses to start. Use this instead:

    python3 web/serve.py 8000 build-web      # then open http://localhost:8000/CnaCraft.html

Any production host works too, as long as it sends those two headers (GitHub
Pages does not; Netlify/Cloudflare/nginx can be configured to).
"""

import functools
import http.server
import socketserver
import sys


class CrossOriginIsolatedHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    directory = sys.argv[2] if len(sys.argv) > 2 else "build-web"
    handler = functools.partial(CrossOriginIsolatedHandler, directory=directory)
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("", port), handler) as httpd:
        print(f"Serving {directory} on http://localhost:{port}/CnaCraft.html "
              f"(cross-origin isolated: wasm threads enabled)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
