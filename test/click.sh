#!/usr/bin/env bash
# click.sh '<aria-label or text prefix>' : clicks the first matching button in the view
label="$1"
node "$(dirname "$0")/cdp.mjs" eval "(()=>{const b=[...document.querySelectorAll('button,a,[role=button]')].find(b=>((b.getAttribute('aria-label')||b.innerText).trim().replace(/\s+/g,' ')).startsWith($(printf '%s' "$label" | python3 -c 'import json,sys;print(json.dumps(sys.stdin.read()))')));if(!b)return 'not found';b.click();return 'clicked'})()" 2>/dev/null
