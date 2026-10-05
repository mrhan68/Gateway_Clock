import re

with open('firmware/main/wifi_provisioning.c', 'r') as f:
    content = f.read()

func = """
void wifi_provisioning_apply_softap_config(void)
{
    s_in_softap_mode = false;
    start_softap_internal();
}
"""

if "wifi_provisioning_apply_softap_config" not in content:
    content += func

with open('firmware/main/wifi_provisioning.c', 'w') as f:
    f.write(content)

with open('firmware/main/wifi_provisioning.h', 'r') as f:
    hcontent = f.read()

hfunc = "void wifi_provisioning_apply_softap_config(void);\n"
if hfunc not in hcontent:
    hcontent = hcontent.replace("#ifdef __cplusplus", hfunc + "\n#ifdef __cplusplus", 1)

with open('firmware/main/wifi_provisioning.h', 'w') as f:
    f.write(hcontent)
