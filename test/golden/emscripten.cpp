// https://emscripten.org/docs/api_reference/emscripten.h.html

EM_JS(void, take_args, (int x, float y), {
  console.log('I received: ' + [x, y]);
});

int main() {
  take_args(100, 35.5);
  return 0;
}

// And then this is from https://github.com/floooh/sokol

EM_JS(void, sapp_js_add_beforeunload_listener, (void), {
  window.addEventListener('beforeunload', Module.sokol_beforeunload);
})
