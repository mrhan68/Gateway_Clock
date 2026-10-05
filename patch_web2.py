import re

with open('firmware/main/web_server.c', 'r') as f:
    content = f.read()

# In post_softap_set_password_handler:
# config_manager_set_softap_password(password);
# ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(password));
# -> add wifi_provisioning_apply_softap_config();
repl = """config_manager_set_softap_password(password);
    ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(password));
    wifi_provisioning_apply_softap_config();"""
content = content.replace('config_manager_set_softap_password(password);\n    ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(password));', repl)

# In post_save_handler:
# config_manager_set_softap_password(softap_pass);
# ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(softap_pass));
# -> add wifi_provisioning_apply_softap_config();
repl2 = """config_manager_set_softap_password(softap_pass);
        ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(softap_pass));
        wifi_provisioning_apply_softap_config();"""
content = content.replace('config_manager_set_softap_password(softap_pass);\n        ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(softap_pass));', repl2)

with open('firmware/main/web_server.c', 'w') as f:
    f.write(content)
