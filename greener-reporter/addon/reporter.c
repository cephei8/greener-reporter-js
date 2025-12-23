#include "reporter.h"

#include <greener_reporter/greener_reporter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void handle_reporter_error(napi_env env,
                                  const struct greener_reporter_error *err) {
  char msg[512];
  snprintf(msg, sizeof(msg), "GreenerReporterError %d/%d: %s", err->code,
           err->ingress_code, err->message);
  napi_throw_error(env, NULL, msg);
  greener_reporter_error_delete(err);
}

static void handle_napi_error(napi_env env) {
  bool pending;
  napi_is_exception_pending(env, &pending);
  if (!pending) {
    const napi_extended_error_info *error_info = NULL;
    napi_get_last_error_info(env, &error_info);
    const char *err_message = error_info->error_message;
    if (err_message == NULL) {
      err_message = "internal error";
    }
    napi_throw_error(env, NULL, err_message);
  }
}

static void reporter_destructor(struct reporter *obj) {
  if (obj->reporter) {
    const struct greener_reporter_error *err;
    greener_reporter_delete(obj->reporter, &err);
  }
  napi_delete_reference(obj->env, obj->wrapper);
  free(obj);
}

void reporter_finalizer(napi_env env, void *obj, void *finalize_hint) {
  (void)finalize_hint;
  reporter_destructor((struct reporter *)obj);
}

static napi_value reporter_ctor(napi_env env, napi_callback_info info);
static napi_value reporter_create_session(napi_env env,
                                          napi_callback_info info);
static napi_value reporter_create_testcase(napi_env env,
                                           napi_callback_info info);
static napi_value reporter_shutdown(napi_env env, napi_callback_info info);

static void instance_data_finalizer(napi_env env, void *data, void *hint) {
  (void)hint;
  napi_ref *ctor_ref = (napi_ref *)data;
  napi_status status = napi_delete_reference(env, *ctor_ref);
  free(ctor_ref);

  if (status != napi_ok) {
    handle_napi_error(env);
  }
}

napi_value reporter_register_class(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      {"createSession", NULL, reporter_create_session, NULL, NULL, NULL,
       napi_default, NULL},
      {"createTestcase", NULL, reporter_create_testcase, NULL, NULL, NULL,
       napi_default, NULL},
      {"shutdown", NULL, reporter_shutdown, NULL, NULL, NULL, napi_default,
       NULL},
  };

  napi_value ctor_value;
  napi_status status = napi_define_class(
      env, "Reporter", NAPI_AUTO_LENGTH, reporter_ctor, NULL,
      sizeof(properties) / sizeof(properties[0]), properties, &ctor_value);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  napi_ref *ctor_ref = (napi_ref *)malloc(sizeof(napi_ref));
  if (!ctor_ref) {
    napi_throw_error(env, NULL, "out of memory");
    return NULL;
  }

  status = napi_create_reference(env, ctor_value, 1, ctor_ref);
  if (status != napi_ok) {
    free(ctor_ref);
    handle_napi_error(env);
    return NULL;
  }

  status = napi_set_instance_data(env, ctor_ref, instance_data_finalizer, NULL);
  if (status != napi_ok) {
    napi_delete_reference(env, *ctor_ref);
    free(ctor_ref);
    handle_napi_error(env);
    return NULL;
  }

  status = napi_set_named_property(env, exports, "Reporter", ctor_value);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  return exports;
}

static napi_value reporter_ctor(napi_env env, napi_callback_info info) {
  napi_value target;
  napi_status status = napi_get_new_target(env, info, &target);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  const size_t argc_num = 2;
  size_t argc = argc_num;
  napi_value args[2];
  napi_value jsthis;
  status = napi_get_cb_info(env, info, &argc, args, &jsthis, NULL);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  if (argc != 2) {
    napi_throw_error(env, NULL, "invalid number of arguments");
    return NULL;
  }

  /* endpoint */
  char server_address[256];
  size_t server_address_len;
  {
    napi_valuetype arg_type;
    status = napi_typeof(env, args[0], &arg_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
    if (arg_type != napi_string) {
      napi_throw_error(env, NULL, "server_address must be a string");
      return NULL;
    }
    status =
        napi_get_value_string_utf8(env, args[0], server_address,
                                   sizeof(server_address), &server_address_len);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
  }

  /* api key */
  char api_key[256];
  size_t api_key_len;
  {
    napi_valuetype arg_type;
    status = napi_typeof(env, args[1], &arg_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
    if (arg_type != napi_string) {
      napi_throw_error(env, NULL, "apiKey must be a string");
      return NULL;
    }
    status = napi_get_value_string_utf8(env, args[1], api_key, sizeof(api_key),
                                        &api_key_len);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
  }

  struct reporter *obj = (struct reporter *)malloc(sizeof(struct reporter));
  if (!obj) {
    napi_throw_error(env, NULL, "out of memory");
    return NULL;
  }

  obj->env = env;
  obj->reporter = NULL;
  obj->wrapper = NULL;

  status = napi_wrap(env, jsthis, obj, reporter_finalizer, NULL, &obj->wrapper);
  if (status != napi_ok) {
    free(obj);
    handle_napi_error(env);
    return NULL;
  }

  const struct greener_reporter_error *err;
  obj->reporter = greener_reporter_new(server_address, api_key, &err);
  if (err != NULL) {
    handle_reporter_error(env, err);
    return NULL;
  }

  return jsthis;
}

static napi_value reporter_create_session(napi_env env,
                                          napi_callback_info info) {
  const size_t argc_num = 4;
  size_t argc = argc_num;
  napi_value args[4];
  napi_value jsthis;
  napi_status status = napi_get_cb_info(env, info, &argc, args, &jsthis, NULL);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  if (argc != 4) {
    napi_throw_error(env, NULL, "invalid number of arguments");
    return NULL;
  }

  char session_id_buf[256];
  char description_buf[256];
  char baggage_buf[256];
  char labels_buf[256];
  const char *in_session_id = NULL;
  const char *in_description = NULL;
  const char *in_baggage = NULL;
  const char *in_labels = NULL;

  /* id */
  {
    napi_valuetype in_type;
    status = napi_typeof(env, args[0], &in_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, NULL, "session_id must be a nullable string");
      return NULL;
    }

    if (in_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[0], session_id_buf,
                                          sizeof(session_id_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_session_id = session_id_buf;
    }
  }

  /* description */
  {
    const size_t arg_idx = 1;
    napi_valuetype in_type;
    status = napi_typeof(env, args[arg_idx], &in_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, NULL, "description must be a nullable string");
      return NULL;
    }

    if (in_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], description_buf,
                                          sizeof(description_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_description = description_buf;
    }
  }

  /* baggage */
  {
    const size_t arg_idx = 2;
    napi_valuetype in_type;
    status = napi_typeof(env, args[arg_idx], &in_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, NULL, "baggage must be a nullable string");
      return NULL;
    }

    if (in_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], baggage_buf,
                                          sizeof(baggage_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_baggage = baggage_buf;
    }
  }

  /* labels */
  {
    const size_t arg_idx = 3;
    napi_valuetype in_type;
    status = napi_typeof(env, args[arg_idx], &in_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, NULL, "labels must be a nullable string");
      return NULL;
    }

    if (in_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], labels_buf,
                                          sizeof(labels_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_labels = labels_buf;
    }
  }

  struct reporter *obj;
  status = napi_unwrap(env, jsthis, (void **)&obj);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  const struct greener_reporter_error *err;
  const struct greener_reporter_session *session =
      greener_reporter_session_create(obj->reporter, in_session_id,
                                      in_description, in_baggage, in_labels,
                                      &err);

  if (err != NULL) {
    handle_reporter_error(env, err);
    return NULL;
  }

  const char *out_session_id = session->id;

  napi_value out_session_id_value;
  status = napi_create_string_utf8(env, out_session_id, strlen(out_session_id),
                                   &out_session_id_value);
  if (status != napi_ok) {
    greener_reporter_session_delete(session);
    handle_napi_error(env);
    return NULL;
  }

  napi_value out_session_value;
  status = napi_create_object(env, &out_session_value);
  if (status != napi_ok) {
    greener_reporter_session_delete(session);
    handle_napi_error(env);
    return NULL;
  }

  status = napi_set_named_property(env, out_session_value, "id",
                                   out_session_id_value);
  if (status != napi_ok) {
    greener_reporter_session_delete(session);
    handle_napi_error(env);
    return NULL;
  }

  greener_reporter_session_delete(session);

  return out_session_value;
}

static napi_value reporter_create_testcase(napi_env env,
                                           napi_callback_info info) {
  const size_t argc_num = 8;
  size_t argc = argc_num;
  napi_value args[8];
  napi_value jsthis;
  napi_status status = napi_get_cb_info(env, info, &argc, args, &jsthis, NULL);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  if (argc != 8) {
    napi_throw_error(env, NULL, "invalid number of arguments");
    return NULL;
  }

  char session_id_buf[256];
  char testcase_name_buf[256];
  char testcase_classname_buf[256];
  char testcase_file_buf[256];
  char testsuite_buf[256];
  char status_buf[256];
  char output_buf[256];
  char baggage_buf[256];

  const char *in_session_id;
  const char *in_testcase_name;
  const char *in_testcase_classname = NULL;
  const char *in_testcase_file = NULL;
  const char *in_testsuite = NULL;
  const char *in_status;
  const char *in_output = NULL;
  const char *in_baggage = NULL;

  /* session_id */
  {
    const size_t arg_idx = 0;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, NULL, "session_id must be a string");
      return NULL;
    }
    size_t buf_len;
    status = napi_get_value_string_utf8(env, args[arg_idx], session_id_buf,
                                        sizeof(session_id_buf), &buf_len);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
    in_session_id = session_id_buf;
  }

  /* testcase_name */
  {
    const size_t arg_idx = 1;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, NULL, "testcase_name must be a string");
      return NULL;
    }
    size_t buf_len;
    status = napi_get_value_string_utf8(env, args[arg_idx], testcase_name_buf,
                                        sizeof(testcase_name_buf), &buf_len);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
    in_testcase_name = testcase_name_buf;
  }

  /* testcase_classname */
  {
    const size_t arg_idx = 2;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, NULL,
                       "testcase_classname must be a nullable string");
      return NULL;
    }

    if (value_type == napi_string) {
      size_t buf_len;
      status =
          napi_get_value_string_utf8(env, args[arg_idx], testcase_classname_buf,
                                     sizeof(testcase_classname_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_testcase_classname = testcase_classname_buf;
    }
  }

  /* testcase_file */
  {
    const size_t arg_idx = 3;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, NULL, "testcase_file must be a nullable string");
      return NULL;
    }

    if (value_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], testcase_file_buf,
                                          sizeof(testcase_file_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_testcase_file = testcase_file_buf;
    }
  }

  /* testsuite */
  {
    const size_t arg_idx = 4;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, NULL, "testsuite must be a nullable string");
      return NULL;
    }

    if (value_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], testsuite_buf,
                                          sizeof(testsuite_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_testsuite = testsuite_buf;
    }
  }

  /* status */
  {
    const size_t arg_idx = 5;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, NULL, "status must be a string");
      return NULL;
    }
    size_t buf_len;
    status = napi_get_value_string_utf8(env, args[arg_idx], status_buf,
                                        sizeof(status_buf), &buf_len);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }
    in_status = status_buf;

    if (strcmp(in_status, "pass") != 0 && strcmp(in_status, "fail") != 0 &&
        strcmp(in_status, "error") != 0 && strcmp(in_status, "skip") != 0) {
      napi_throw_error(env, NULL, "status must be one of pass|fail|err|skip");
      return NULL;
    }
  }

  /* output */
  {
    const size_t arg_idx = 6;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, NULL, "stdout_stream must be a nullable string");
      return NULL;
    }

    if (value_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], output_buf,
                                          sizeof(output_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_output = output_buf;
    }
  }

  /* baggage */
  {
    const size_t arg_idx = 7;
    napi_valuetype value_type;
    status = napi_typeof(env, args[arg_idx], &value_type);
    if (status != napi_ok) {
      handle_napi_error(env);
      return NULL;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, NULL, "baggage must be a nullable string");
      return NULL;
    }

    if (value_type == napi_string) {
      size_t buf_len;
      status = napi_get_value_string_utf8(env, args[arg_idx], baggage_buf,
                                          sizeof(baggage_buf), &buf_len);
      if (status != napi_ok) {
        handle_napi_error(env);
        return NULL;
      }
      in_baggage = baggage_buf;
    }
  }

  struct reporter *obj;
  status = napi_unwrap(env, jsthis, (void **)&obj);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  const struct greener_reporter_error *err;
  greener_reporter_testcase_create(
      obj->reporter, in_session_id, in_testcase_name, in_testcase_classname,
      in_testcase_file, in_testsuite, in_status, in_output, in_baggage, &err);

  if (err != NULL) {
    handle_reporter_error(env, err);
    return NULL;
  }

  return NULL;
}

static napi_value reporter_shutdown(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value jsthis;
  napi_status status = napi_get_cb_info(env, info, &argc, NULL, &jsthis, NULL);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  if (argc != 0) {
    napi_throw_error(env, NULL, "no arguments expected");
    return NULL;
  }

  struct reporter *obj;
  status = napi_unwrap(env, jsthis, (void **)&obj);
  if (status != napi_ok) {
    handle_napi_error(env);
    return NULL;
  }

  const struct greener_reporter_error *err;
  greener_reporter_delete(obj->reporter, &err);
  obj->reporter = NULL;

  if (err != NULL) {
    handle_reporter_error(env, err);
    return NULL;
  }

  return NULL;
}
