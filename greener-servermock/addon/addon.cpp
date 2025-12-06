#include "./servermock.hpp"

napi_value register_addon(napi_env env, napi_value exports) {
  return servermock::register_class(env, exports);
}

NAPI_MODULE(NODE_GYP_MODULE_NAME,  register_addon)