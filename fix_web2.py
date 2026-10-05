import re

with open('firmware/main/web_server.c', 'r') as f:
    content = f.read()

def repl_ssid(match):
    s = match.group(0)
    return """char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));"""

def repl_pass(match):
    s = match.group(0)
    return """char raw_pass[65] = {0};
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));"""

content = re.sub(r'strncpy\(ssid,\s*p_ssid,\s*len\);', repl_ssid, content)
content = re.sub(r'strncpy\(password,\s*p_pass,\s*len\);', repl_pass, content)

with open('firmware/main/web_server.c', 'w') as f:
    f.write(content)
