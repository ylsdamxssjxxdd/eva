# command.wasm

This is a basic Voice Assistant example that accepts voice commands from the microphone.
It runs in fully in the browser via WebAseembly.

Online demo: https://ggml.ai/whisper.cpp/command.wasm/

Terminal version: [examples/command](/examples/command)

## Build instructions

```bash
# build using Emscripten (v3.1.2)
git clone https://github.com/ggerganov/whisper.cpp
cd whisper.cpp
mkdir build-em && cd build-em
emcmake cmake ..
make -j libcommand
```
The example can then be started by running a local HTTP server:
```console
python3 examples/server.py
```
And then opening a browser to the following URL:
http://localhost:8000/command.wasm/

To run the example in a different server, you need to copy the following files
to the server's HTTP path:
```
cp bin/command.wasm/*       /path/to/html/
cp bin/libcommand.js        /path/to/html/
cp bin/libcommand.worker.js /path/to/html/
```

> 📝 **Note:** By default this example is built with `WHISPER_WASM_SINGLE_FILE=ON`
> which means that that a separate .wasm file will not be generated. Instead, the
> WASM module is embedded in the main JS file as a base64 encoded string. To
> generate a separate .wasm file, you need to disable this option by passing
> `-DWHISPER_WASM_SINGLE_FILE=OFF`:
> ```console
> emcmake cmake .. -DWHISPER_WASM_SINGLE_FILE=OFF
> ```
> This will generate a `libcommand.wasm` file in the build/bin directory.

> 📝 **Note:** As of Emscripten 3.1.58 (April 2024), separate worker.js files are no
> longer generated and the worker is embedded in the main JS file. So the worker
> file will not be geneated for versions later than `3.1.58`.
