#ifndef GREENER_REPORTER_ADDON_REPORTER_H
#define GREENER_REPORTER_ADDON_REPORTER_H

#include <node_api.h>

struct greener_reporter;

struct reporter {
  struct greener_reporter *reporter;
  napi_env env;
  napi_ref wrapper;
};

napi_value reporter_register_class(napi_env env, napi_value exports);
void reporter_finalizer(napi_env env, void *obj, void *finalize_hint);

#endif /* GREENER_REPORTER_ADDON_REPORTER_H */
