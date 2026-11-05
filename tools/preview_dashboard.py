"""Apercu local de la page embarquee, avec donnees de test explicites."""
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'firmware/EnviroTerminalDiagnostic/WebDashboardPage.h'
STATE = ROOT / '.tools/dashboard-preview-status.json'

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            page = HEADER.read_text(encoding='utf-8').split('R"HTML(', 1)[1].rsplit(')HTML";', 1)[0]
            page = page.replace('<body>', '<body><div style="padding:8px;text-align:center;background:#4b3a19;color:#fff">APERÇU · données de test, pas de connexion au capteur</div>')
            content, kind = page.encode(), 'text/html; charset=utf-8'
        elif self.path == '/api/status':
            content, kind = json.dumps(json.loads(STATE.read_text(encoding='utf-8-sig'))).encode(), 'application/json'
        else:
            self.send_error(404); return
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(content)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(content)

if __name__ == '__main__':
    print('Apercu de test : http://127.0.0.1:8765/', flush=True)
    HTTPServer(('127.0.0.1', 8765), Handler).serve_forever()
