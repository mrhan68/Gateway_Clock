import re

with open('firmware/main/web_server.c', 'r') as f:
    content = f.read()

# Fix post_softap_set_password_handler
def fix_handler(match):
    body = match.group(0)
    if 'url_decode' not in body:
        # replace strncpy(password, p_pass, len); with temp buffer and decode
        repl = """char raw_pass[65] = {0};
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));"""
        body = re.sub(r'strncpy\(password,\s*p_pass,\s*len\);', repl, body)
    return body

content = re.sub(r'static esp_err_t post_softap_set_password_handler.*?^\}', fix_handler, content, flags=re.MULTILINE|re.DOTALL)

# Fix post_wifi_save_handler
def fix_save_handler(match):
    body = match.group(0)
    if 'url_decode' not in body:
        repl_ssid = """char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));"""
        body = re.sub(r'strncpy\(ssid,\s*p_ssid,\s*len\);', repl_ssid, body)
        
        repl_pass = """char raw_pass[65] = {0};
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));"""
        body = re.sub(r'strncpy\(password,\s*p_pass,\s*len\);', repl_pass, body)
    return body

content = re.sub(r'static esp_err_t post_wifi_save_handler.*?^\}', fix_save_handler, content, flags=re.MULTILINE|re.DOTALL)

# Fix post_wifi_connect_handler
def fix_conn_handler(match):
    body = match.group(0)
    if 'url_decode' not in body:
        repl_ssid = """char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));"""
        body = re.sub(r'strncpy\(ssid,\s*p_ssid,\s*len\);', repl_ssid, body)
    return body

content = re.sub(r'static esp_err_t post_wifi_connect_handler.*?^\}', fix_conn_handler, content, flags=re.MULTILINE|re.DOTALL)

# Fix post_wifi_delete_handler
content = re.sub(r'static esp_err_t post_wifi_delete_handler.*?^\}', fix_conn_handler, content, flags=re.MULTILINE|re.DOTALL)

with open('firmware/main/web_server.c', 'w') as f:
    f.write(content)
