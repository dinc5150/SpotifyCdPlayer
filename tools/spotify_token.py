"""Get a Spotify refresh token on a PC for spike S4 (PKCE, loopback redirect).

Register http://127.0.0.1:8888/callback as a redirect URI in your Spotify app,
then run (PlatformIO's Python works; no extra packages needed):

    %USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe tools\\spotify_token.py <client_id>

A browser opens for the Spotify login. The script prints a `token ...` line to
paste into the S3/S4 spike's serial console.
"""

import base64
import hashlib
import http.server
import json
import secrets
import ssl
import sys
import urllib.parse
import urllib.request
import webbrowser

REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = " ".join([
    "user-read-playback-state",
    "user-modify-playback-state",
    "user-read-currently-playing",
    "user-read-private",
    "playlist-read-private",
    "playlist-read-collaborative",
    "user-library-read",
    "user-read-playback-position",
])


def tls_context() -> ssl.SSLContext:
    # Python on Windows verifies against the Windows store, which only fetches
    # root certificates on demand and may lack Spotify's. PlatformIO's Python
    # ships certifi, so prefer its bundle.
    try:
        import certifi
        return ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        return ssl.create_default_context()


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Usage: spotify_token.py <client_id>")
    client_id = sys.argv[1]
    verifier = secrets.token_urlsafe(48)[:64]
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    state = secrets.token_urlsafe(12)
    result: dict = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802 (http.server API)
            query = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            result.update({k: v[0] for k, v in query.items()})
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"Done. You can close this tab and return to the terminal.")

        def log_message(self, *args):
            pass

    authorize = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "code_challenge_method": "S256",
        "code_challenge": challenge,
        "scope": SCOPES,
        "state": state,
    })
    print("Opening the Spotify login in your browser. If it doesn't open, visit:\n" + authorize)
    webbrowser.open(authorize)
    with http.server.HTTPServer(("127.0.0.1", 8888), Handler) as server:
        while "code" not in result and "error" not in result:
            server.handle_request()

    if "error" in result:
        sys.exit(f"Login failed: {result['error']}")
    if result.get("state") != state:
        sys.exit("State mismatch; aborting.")

    form = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": result["code"],
        "redirect_uri": REDIRECT_URI,
        "client_id": client_id,
        "code_verifier": verifier,
    }).encode()
    request = urllib.request.Request("https://accounts.spotify.com/api/token", data=form,
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
    with urllib.request.urlopen(request, context=tls_context()) as response:
        tokens = json.load(response)

    print("\nPaste this into the S3/S4 spike console:\n")
    print(f"token {client_id} {tokens['refresh_token']}\n")
    print("Keep it private: it controls playback on your account until revoked or expired (6 months).")


if __name__ == "__main__":
    main()
