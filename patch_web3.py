with open('firmware/main/web_server.c', 'r') as f:
    content = f.read()

repl3 = """config_manager_set_softap_password("");
        wifi_provisioning_apply_softap_config();"""
content = content.replace('config_manager_set_softap_password("");', repl3)

with open('firmware/main/web_server.c', 'w') as f:
    f.write(content)
