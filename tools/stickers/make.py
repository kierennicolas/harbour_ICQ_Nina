# Original sticker pack for ICQ Nina: "Pip", a round little creature, in a
# dozen moods. Drawn from scratch as SVG.
import os, sys, cairosvg
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)

BODY = "#4fc98a"; BODY_DARK = "#2e9e68"; CHEEK = "#ff8fa3"; INK = "#1d3b2f"

def body(color=BODY, dark=BODY_DARK):
    return f'''
  <ellipse cx="128" cy="232" rx="70" ry="10" fill="#000" opacity="0.15"/>
  <path d="M128 40 C 196 40 222 92 222 146 C 222 200 186 228 128 228 C 70 228 34 200 34 146 C 34 92 60 40 128 40 Z"
        fill="{color}" stroke="#fff" stroke-width="10" stroke-linejoin="round"/>
  <path d="M60 120 C 64 82 92 60 120 56" stroke="#fff" stroke-width="8" stroke-linecap="round" fill="none" opacity="0.55"/>
  <ellipse cx="96" cy="214" rx="20" ry="10" fill="{dark}"/>
  <ellipse cx="160" cy="214" rx="20" ry="10" fill="{dark}"/>'''

def eyes(kind="open"):
    if kind == "open":
        return f'''<circle cx="100" cy="128" r="12" fill="{INK}"/><circle cx="156" cy="128" r="12" fill="{INK}"/>
  <circle cx="104" cy="123" r="4" fill="#fff"/><circle cx="160" cy="123" r="4" fill="#fff"/>'''
    if kind == "happy":
        return f'''<path d="M86 132 Q100 114 114 132" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>
  <path d="M142 132 Q156 114 170 132" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>'''
    if kind == "closed":
        return f'''<path d="M86 128 Q100 140 114 128" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>
  <path d="M142 128 Q156 140 170 128" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>'''
    if kind == "wink":
        return f'''<circle cx="100" cy="128" r="12" fill="{INK}"/><circle cx="104" cy="123" r="4" fill="#fff"/>
  <path d="M142 130 Q156 118 170 130" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>'''
    if kind == "big":
        return f'''<circle cx="100" cy="124" r="20" fill="#fff"/><circle cx="156" cy="124" r="20" fill="#fff"/>
  <circle cx="100" cy="126" r="11" fill="{INK}"/><circle cx="156" cy="126" r="11" fill="{INK}"/>'''
    if kind == "angry":
        return f'''<circle cx="102" cy="132" r="11" fill="{INK}"/><circle cx="154" cy="132" r="11" fill="{INK}"/>
  <path d="M82 108 L118 120" stroke="{INK}" stroke-width="9" stroke-linecap="round"/>
  <path d="M174 108 L138 120" stroke="{INK}" stroke-width="9" stroke-linecap="round"/>'''
    if kind == "sad":
        return f'''<circle cx="100" cy="132" r="11" fill="{INK}"/><circle cx="156" cy="132" r="11" fill="{INK}"/>
  <path d="M84 112 L112 104" stroke="{INK}" stroke-width="7" stroke-linecap="round"/>
  <path d="M172 112 L144 104" stroke="{INK}" stroke-width="7" stroke-linecap="round"/>'''
    if kind == "hearts":
        h = lambda x: f'<path d="M{x} 136 C{x-22} 120 {x-14} 104 {x} 116 C{x+14} 104 {x+22} 120 {x} 136 Z" fill="#ff4d6d"/>'
        return h(100) + h(156)

def cheeks():
    return f'<ellipse cx="78" cy="156" rx="12" ry="8" fill="{CHEEK}" opacity="0.8"/><ellipse cx="178" cy="156" rx="12" ry="8" fill="{CHEEK}" opacity="0.8"/>'

def mouth(kind):
    m = {
        "smile": f'<path d="M110 160 Q128 178 146 160" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>',
        "grin": f'<path d="M104 156 Q128 196 152 156 Z" fill="{INK}"/><path d="M114 172 Q128 184 142 172 Z" fill="#ff7a90"/>',
        "o": f'<ellipse cx="128" cy="166" rx="12" ry="15" fill="{INK}"/>',
        "flat": f'<path d="M112 166 L144 166" stroke="{INK}" stroke-width="8" stroke-linecap="round"/>',
        "frown": f'<path d="M110 172 Q128 154 146 172" stroke="{INK}" stroke-width="8" fill="none" stroke-linecap="round"/>',
        "tiny": f'<path d="M120 162 Q128 170 136 162" stroke="{INK}" stroke-width="7" fill="none" stroke-linecap="round"/>',
    }
    return m[kind]

def outlined(x, y, text, size, color, anchor="middle", width=8):
    common = f'x="{x}" y="{y}" text-anchor="{anchor}" font-family="DejaVu Sans, Arial, sans-serif" font-weight="bold" font-size="{size}"'
    return (f'<text {common} fill="#fff" stroke="#fff" stroke-width="{width}" stroke-linejoin="round">{text}</text>'
            f'<text {common} fill="{color}">{text}</text>')

def caption(text, color="#ff9b26"):
    return outlined(128, 36, text, 34, color)

def arm_wave():
    return f'''<path d="M206 120 Q236 96 232 64" stroke="{BODY}" stroke-width="22" stroke-linecap="round" fill="none"/>
  <path d="M206 120 Q236 96 232 64" stroke="#fff" stroke-width="4" stroke-linecap="round" fill="none" opacity="0.0"/>
  <path d="M240 50 l10 -6 M244 64 l12 0 M238 36 l6 -10" stroke="#ff9b26" stroke-width="5" stroke-linecap="round"/>'''

def thumb():
    return f'''<g transform="translate(196 132)"><rect x="-6" y="0" width="40" height="34" rx="12" fill="{BODY}" stroke="#fff" stroke-width="6"/>
  <rect x="2" y="-30" width="16" height="38" rx="8" fill="{BODY}" stroke="#fff" stroke-width="6"/></g>'''

def tears():
    return '<path d="M96 146 Q90 170 98 182 Q106 170 100 146 Z" fill="#5ab8ff"/><path d="M160 146 Q154 170 162 182 Q170 170 164 146 Z" fill="#5ab8ff"/>'

def zzz():
    return outlined(196, 72, "z", 30, "#7ab8ff", "start", 6) + outlined(214, 48, "z", 22, "#7ab8ff", "start", 5)

def confetti():
    cols = ["#ff4d6d", "#ffd23f", "#5ab8ff", "#b47bff", "#ff9b26"]
    pts = [(40,60),(70,30),(200,40),(226,90),(30,110),(220,150),(52,190)]
    return "".join(f'<rect x="{x}" y="{y}" width="12" height="7" rx="2" fill="{cols[i%5]}" transform="rotate({i*37} {x} {y})"/>' for i,(x,y) in enumerate(pts))

def party_hat():
    return '<path d="M128 6 L104 56 L152 56 Z" fill="#b47bff" stroke="#fff" stroke-width="6" stroke-linejoin="round"/><circle cx="128" cy="8" r="9" fill="#ffd23f" stroke="#fff" stroke-width="4"/>'

def heart_big():
    return '<path d="M206 70 C190 52 166 66 182 90 L206 112 L230 90 C246 66 222 52 206 70 Z" fill="#ff4d6d" stroke="#fff" stroke-width="6"/>'

def steam_cup():
    return f'''<g transform="translate(150 150)"><rect x="0" y="10" width="56" height="50" rx="10" fill="#fff" stroke="{INK}" stroke-width="5"/>
  <path d="M56 22 q18 4 0 26" stroke="{INK}" stroke-width="5" fill="none"/><rect x="6" y="16" width="44" height="12" rx="4" fill="#9c6b3f"/>
  <path d="M16 0 q-8 -12 2 -22 M32 0 q-8 -12 2 -22" stroke="#bbb" stroke-width="4" fill="none" stroke-linecap="round"/></g>'''

def question():
    return outlined(200, 92, "?", 64, "#ffb300", "start", 8)

def sweat():
    return '<path d="M188 92 Q178 112 188 122 Q198 112 188 92 Z" fill="#5ab8ff" stroke="#fff" stroke-width="3"/>'

stickers = [
    ("hello",   "Hi!",    body() + eyes("open") + cheeks() + mouth("grin") + arm_wave()),
    ("lol",     "LOL",    body() + eyes("happy") + cheeks() + mouth("grin")),
    ("love",    "",       body() + eyes("hearts") + cheeks() + mouth("smile") + heart_big()),
    ("thanks",  "Thanks", body() + eyes("closed") + cheeks() + mouth("smile")),
    ("ok",      "OK!",    body() + eyes("wink") + cheeks() + mouth("smile") + thumb()),
    ("wow",     "Wow",    body() + eyes("big") + mouth("o")),
    ("sad",     "",       body("#7ac4a0", "#4f9a78") + eyes("sad") + mouth("frown") + tears()),
    ("angry",   "Grr",    body("#ff8a6b", "#d9654a") + eyes("angry") + mouth("flat")),
    ("sleepy",  "",       body() + eyes("closed") + mouth("tiny") + zzz()),
    ("party",   "",       confetti() + body() + eyes("happy") + cheeks() + mouth("grin") + party_hat()),
    ("coffee",  "",       body() + eyes("open") + cheeks() + mouth("tiny") + steam_cup()),
    ("what",    "",       body() + eyes("big") + mouth("flat") + question() + sweat()),
]

for name, text, art in stickers:
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">{art}{caption(text) if text else ""}</svg>'
    open(os.path.join(OUT, name + ".svg"), "w").write(svg)
    cairosvg.svg2png(bytestring=svg.encode(), write_to=os.path.join(OUT, name + ".png"), output_width=256, output_height=256)
print(len(stickers), "stickers")
