# Local build corrections

- Added forward declarations required by the C++ compiler.
- Replaced transparent TFT_eSprite pushImage calls with keyed RGB565 pixel compositing into the existing 8-bit stage buffer.
- Selected min_spiffs.csv: the 1,576,481-byte program exceeds the default 1,310,720-byte app partition. The new app partition is 1,966,080 bytes. NVS remains at offset 0x9000 with size 0x5000, preserving pet state and touch calibration.
