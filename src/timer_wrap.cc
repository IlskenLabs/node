#include <node.h>
#include <handle_wrap.h>

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TimerWrap* wrap =  \
      static_cast<TimerWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    SetErrno(UV_EBADF); \
    return scope.Close(Integer::New(-1)); \
  }

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;


class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Timer"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);

    target->Set(String::NewSymbol("Timer"), constructor->GetFunction());
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    TimerWrap *wrap = new TimerWrap(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

  TimerWrap(Handle<Object> object)
      : HandleWrap(object, (uv_handle_t*) &handle_) {
    active_ = false;

    int r = uv_timer_init(uv_default_loop(), &handle_);
    assert(r == 0);

    handle_.data = this;

    // uv_timer_init adds a loop reference. (That is, it calls uv_ref.) This
    // is not the behavior we want in Node. Timers should not increase the
    // ref count of the loop except when active.
    uv_unref(uv_default_loop());
  }

  ~TimerWrap() {
    if (!active_) uv_ref(uv_default_loop());
  }

  void StateChange() {
    bool was_active = active_;
    active_ = uv_is_active((uv_handle_t*) &handle_);

    if (!was_active && active_) {
      // If our state is changing from inactive to active, we
      // increase the loop's reference count.
      uv_ref(uv_default_loop());
    } else if (was_active && !active_) {
      // If our state is changing from active to inactive, we
      // decrease the loop's reference count.
      uv_unref(uv_default_loop());
    }
  }

  static Handle<Value> Start(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();

    int r = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);

    // Error starting the timer.
    if (r) SetErrno(uv_last_error(uv_default_loop()).code);

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Stop(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_stop(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()).code);

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Again(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_again(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()).code);

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> SetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t repeat = args[0]->IntegerValue();

    uv_timer_set_repeat(&wrap->handle_, repeat);

    return scope.Close(Integer::New(0));
  }

  static Handle<Value> GetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t repeat = uv_timer_get_repeat(&wrap->handle_);

    if (repeat < 0) SetErrno(uv_last_error(uv_default_loop()).code);

    return scope.Close(Integer::New(repeat));
  }

  static void OnTimeout(uv_timer_t* handle, int status) {
    HandleScope scope;

    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    assert(wrap);

    wrap->StateChange();

    Local<Value> argv[1] = { Integer::New(status) };
    MakeCallback(wrap->object_, "ontimeout", 1, argv);
  }

  uv_timer_t handle_;
  // This member is set false initially. When the timer is turned
  // on uv_ref is called. When the timer is turned off uv_unref is
  // called. Used to mirror libev semantics.
  bool active_;
};


}  // namespace node

NODE_MODULE(node_timer_wrap, node::TimerWrap::Initialize);
