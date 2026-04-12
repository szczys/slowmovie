# Patches

These patches need to be applied to build with Arduino as a component:

- insights.patch
  - managed_components/espressif__esp_rainmaker/CMakeLists.txt
- rainmaker.patch
  - managed_components/espressif__esp_insights/CMakeLists.txt

This resolves build error described in this
[issue](https://github.com/espressif/esp-rainmaker/pull/346).
