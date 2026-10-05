// Node.js binding of the ECR17 core: a thin Node-API layer over padosoft::ecr17::Ecr17Client.
//
// No protocol logic lives here. Requests arrive already normalized by the TypeScript layer
// (src/client.ts), results leave as the core's raw response structs (strings), and
// src/mappers.ts turns them into the public result types, the same ones as
// @padosoft/react-native-ecr17.
//
// Threading: every command blocks (a payment waits for the cardholder), so each client owns
// one worker thread that runs its commands in order, off the event loop and off libuv's
// thread pool. Results and events come back to JavaScript through ONE thread-safe function
// per client, in order: a command's events always arrive before its result.
#include <napi.h>

#include <cmath>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "ecr17/Ecr17Client.hpp"

#if defined(_WIN32)
#include "ecr17/WinsockTransport.hpp"
using PlatformTransport = padosoft::ecr17::WinsockTransport;
#else
#include "ecr17/PosixTransport.hpp"
using PlatformTransport = padosoft::ecr17::PosixTransport;
#endif

namespace core = padosoft::ecr17;

namespace {

// ---------- JS -> core ----------

Napi::Value field(const Napi::Object& object, const char* key) {
    return object.Has(key) ? object.Get(key) : object.Env().Undefined();
}

std::string getString(const Napi::Object& object, const char* key, const std::string& fallback = "") {
    const Napi::Value value = field(object, key);
    if (value.IsUndefined() || value.IsNull()) return fallback;
    if (!value.IsString()) throw std::invalid_argument(std::string("ECR17: ") + key + " must be a string");
    return value.As<Napi::String>().Utf8Value();
}

std::optional<std::string> getOptString(const Napi::Object& object, const char* key) {
    const Napi::Value value = field(object, key);
    if (value.IsUndefined() || value.IsNull()) return std::nullopt;
    return getString(object, key);
}

// JS numbers are doubles. NaN, infinities and out-of-range values throw instead of the
// undefined behaviour of a plain cast; fractions are truncated (as in the RN binding).
int getInt(const Napi::Object& object, const char* key, int fallback) {
    const Napi::Value value = field(object, key);
    if (value.IsUndefined() || value.IsNull()) return fallback;
    if (!value.IsNumber()) throw std::invalid_argument(std::string("ECR17: ") + key + " must be a number");
    const double number = value.As<Napi::Number>().DoubleValue();
    if (!std::isfinite(number) || number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(std::string("ECR17: ") + key + " is not a valid number");
    }
    return static_cast<int>(number);
}

bool getBool(const Napi::Object& object, const char* key, bool fallback) {
    const Napi::Value value = field(object, key);
    if (value.IsUndefined() || value.IsNull()) return fallback;
    if (!value.IsBoolean()) throw std::invalid_argument(std::string("ECR17: ") + key + " must be a boolean");
    return value.As<Napi::Boolean>().Value();
}

core::LrcMode toLrcMode(const std::string& mode) {
    if (mode == "stx") return core::LrcMode::STX;
    if (mode == "noext") return core::LrcMode::NOEXT;
    if (mode == "stx_noext") return core::LrcMode::STX_NOEXT;
    if (mode == "std" || mode.empty()) return core::LrcMode::STD;
    throw std::invalid_argument("ECR17: unknown lrcMode '" + mode + "'");
}

core::PaymentCardType toCardType(const std::string& type) {
    if (type == "debit") return core::PaymentCardType::Debit;
    if (type == "credit") return core::PaymentCardType::Credit;
    if (type == "other") return core::PaymentCardType::Other;
    if (type == "auto" || type.empty()) return core::PaymentCardType::Auto;
    throw std::invalid_argument("ECR17: unknown paymentType '" + type + "'");
}

core::ClientConfig toConfig(const Napi::Object& c) {
    core::ClientConfig k;
    k.host = getString(c, "host");
    k.port = getInt(c, "port", 10000);
    k.connectionTimeoutMs = getInt(c, "connectionTimeoutMs", 5000);
    k.terminalId = getString(c, "terminalId");
    k.cashRegisterId = getString(c, "cashRegisterId");
    k.lrcMode = toLrcMode(getString(c, "lrcMode", "std"));
    k.ackTimeoutMs = getInt(c, "ackTimeoutMs", 2000);
    k.responseTimeoutMs = getInt(c, "responseTimeoutMs", 60000);
    k.retryCount = getInt(c, "retryCount", 3);
    k.retryDelayMs = getInt(c, "retryDelayMs", 200);
    k.receiptDrainMs = getInt(c, "receiptDrainMs", 0);
    k.autoReconnect = getBool(c, "autoReconnect", false);
    return k;
}

std::optional<core::TokenizationRequest> toTokenization(const Napi::Object& r) {
    const Napi::Value value = field(r, "tokenization");
    if (value.IsUndefined() || value.IsNull()) return std::nullopt;
    if (!value.IsObject()) throw std::invalid_argument("ECR17: tokenization must be an object");
    const Napi::Object t = value.As<Napi::Object>();
    const std::string service = getString(t, "service");
    if (service != "recurring" && service != "unscheduledOrOneClick") {
        throw std::invalid_argument("ECR17: unknown tokenization service '" + service + "'");
    }
    return core::TokenizationRequest{service == "recurring" ? core::TokenizationService::Recurring
                                                            : core::TokenizationService::UnscheduledOrOneClick,
                                     getString(t, "contractCode")};
}

core::PaymentRequest toPayment(const Napi::Object& r) {
    core::PaymentRequest k;
    k.amountCents = getInt(r, "amountCents", 0);
    k.cashRegisterId = getOptString(r, "cashRegisterId");
    k.paymentType = toCardType(getString(r, "paymentType", "auto"));
    k.cardAlreadyPresent = getBool(r, "cardAlreadyPresent", false);
    k.receiptText = getString(r, "receiptText");
    k.tokenization = toTokenization(r);
    return k;
}

core::PreAuthFollowUpRequest toFollowUp(const Napi::Object& r) {
    core::PreAuthFollowUpRequest k;
    k.amountCents = getInt(r, "amountCents", 0);
    k.originalPreAuthCode = getString(r, "originalPreAuthCode");
    k.cashRegisterId = getOptString(r, "cashRegisterId");
    k.receiptText = getString(r, "receiptText");
    return k;
}

core::ReversalRequest toReversal(const Napi::Object& r) {
    core::ReversalRequest k;
    k.cashRegisterId = getOptString(r, "cashRegisterId");
    k.stan = getString(r, "stan", "000000");
    return k;
}

core::CardVerificationRequest toVerification(const Napi::Object& r) {
    core::CardVerificationRequest k;
    k.cashRegisterId = getOptString(r, "cashRegisterId");
    k.paymentType = toCardType(getString(r, "paymentType", "auto"));
    k.tokenization = toTokenization(r);
    return k;
}

// ---------- core -> JS (raw structs; src/mappers.ts builds the public types) ----------

const char* outcomeName(core::Outcome outcome) {
    switch (outcome) {
        case core::Outcome::Ok: return "ok";
        case core::Outcome::Ko: return "ko";
        case core::Outcome::CardNotPresent: return "cardNotPresent";
        case core::Outcome::UnknownTag: return "unknownTag";
        case core::Outcome::Unknown: break;
    }
    return "unknown";
}

const char* stateName(core::ConnectionState state) {
    switch (state) {
        case core::ConnectionState::Connecting: return "connecting";
        case core::ConnectionState::Connected: return "connected";
        case core::ConnectionState::Disconnected: break;
    }
    return "disconnected";
}

Napi::Object toJs(Napi::Env env, const core::PaymentResponse& p) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("outcome", outcomeName(p.outcome));
    o.Set("resultCode", p.resultCode);
    o.Set("pan", p.pan);
    o.Set("transactionType", p.transactionType);
    o.Set("authCode", p.authCode);
    o.Set("hostDateTime", p.hostDateTime);
    o.Set("errorDescription", p.errorDescription);
    o.Set("cardType", p.cardType);
    o.Set("acquirerId", p.acquirerId);
    o.Set("stan", p.stan);
    o.Set("onlineId", p.onlineId);
    Napi::Object dcc = Napi::Object::New(env);
    dcc.Set("applied", p.currency.applied);
    dcc.Set("rate", p.currency.rate);
    dcc.Set("currencyCode", p.currency.currencyCode);
    dcc.Set("amount", p.currency.amount);
    dcc.Set("precision", p.currency.precision);
    o.Set("currency", dcc);
    return o;
}

Napi::Object toJs(Napi::Env env, const core::PreAuthResponse& p) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("outcome", outcomeName(p.outcome));
    o.Set("resultCode", p.resultCode);
    o.Set("pan", p.pan);
    o.Set("transactionType", p.transactionType);
    o.Set("authCode", p.authCode);
    o.Set("preAuthorizedAmount", p.preAuthorizedAmount);
    o.Set("preAuthCode", p.preAuthCode);
    o.Set("actionCode", p.actionCode);
    o.Set("hostDateTime", p.hostDateTime);
    o.Set("errorDescription", p.errorDescription);
    o.Set("cardType", p.cardType);
    o.Set("acquirerId", p.acquirerId);
    o.Set("stan", p.stan);
    o.Set("onlineId", p.onlineId);
    return o;
}

Napi::Object toJs(Napi::Env env, const core::StatusResponse& s) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("terminalId", s.terminalId);
    o.Set("dateTimeRaw", s.dateTimeRaw);
    o.Set("status", s.status);
    o.Set("softwareRelease", s.softwareRelease);
    return o;
}

Napi::Object toJs(Napi::Env env, const core::TotalsResponse& t) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("outcome", outcomeName(t.outcome));
    o.Set("resultCode", t.resultCode);
    o.Set("posTotal", t.posTotal);
    return o;
}

Napi::Object toJs(Napi::Env env, const core::CloseResponse& c) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("outcome", outcomeName(c.outcome));
    o.Set("resultCode", c.resultCode);
    o.Set("posTotal", c.posTotal);
    o.Set("hostTotal", c.hostTotal);
    o.Set("errorDescription", c.errorDescription);
    o.Set("actionCode", c.actionCode);
    return o;
}

Napi::Object toJs(Napi::Env env, const core::VasResponse& v) {
    Napi::Object o = Napi::Object::New(env);
    o.Set("responseId", v.responseId);
    o.Set("responseMessage", v.responseMessage);
    o.Set("orderId", v.orderId);
    o.Set("rawXml", v.rawXml);
    return o;
}

Napi::Error toJsError(Napi::Env env, const std::string& message, bool invalidArgument) {
    Napi::Error error = Napi::Error::New(env, message);
    error.Set("code", invalidArgument ? "ECR17_INVALID_ARGUMENT" : "ECR17_COMMAND_FAILED");
    return error;
}

// ---------- delivery to the main thread ----------

// Builds the JS value of a finished command on the main thread.
using Materializer = std::function<Napi::Value(Napi::Env)>;

struct Worker;

struct Completion {
    std::shared_ptr<Worker> worker;                // for the pending count; outlives the thread
    std::unique_ptr<Napi::Promise::Deferred> deferred;
    std::shared_ptr<Napi::ObjectReference> owner;  // keeps the JS client alive meanwhile
    Materializer value;                            // set on success
    std::string error;                             // set on failure
    bool invalidArgument = false;
};

// The JS listeners. Touched only on the main thread; other threads hold a weak_ptr, so
// the function references are always released on the main thread.
struct Listeners {
    Napi::FunctionReference progress;
    Napi::FunctionReference receipt;
    Napi::FunctionReference state;
};

// One delivery to the main thread: a finished command or an event. Both travel through
// ONE FIFO queue per client, so the events of a command (progress, receipt lines,
// connection state) always reach JavaScript before its promise settles.
struct Delivery {
    std::unique_ptr<Completion> completion;  // a finished command, or
    std::function<void(Napi::Env)> event;    // an event (looks its listener up)
};

void deliver(Napi::Env env, Napi::Function, std::nullptr_t*, Delivery* delivery);
using MainQueue = Napi::TypedThreadSafeFunction<std::nullptr_t, Delivery, deliver>;

struct Job {
    std::function<Materializer()> run;  // runs on the worker thread
    std::unique_ptr<Completion> completion;
};

// The worker thread, its command queue and the client's main-thread queue. Shared with the
// thread, so closing the client never has to join a thread that may be waiting for a
// cardholder.
struct Worker {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Job> queue;
    bool stopping = false;

    std::mutex postMutex;
    bool released = false;  // the main queue was released: posts are dropped
    MainQueue main;
    // Commands submitted and not yet delivered (main thread only): the main queue keeps
    // the process alive only while this is > 0.
    int pending = 0;

    // Any thread. False if the queue is gone (client closed, environment shutting down).
    bool post(Delivery* delivery) {
        std::lock_guard<std::mutex> lock(postMutex);
        return !released && main.NonBlockingCall(delivery) == napi_ok;
    }

    void postEvent(std::function<void(Napi::Env)> event) {
        auto* delivery = new Delivery{nullptr, std::move(event)};
        if (!post(delivery)) delete delivery;  // holds no JS handles: safe on any thread
    }

    static void loop(std::shared_ptr<Worker> self) {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(self->mutex);
                self->changed.wait(lock, [&]() { return self->stopping || !self->queue.empty(); });
                if (self->queue.empty()) break;  // stopping, nothing left to run
                job = std::move(self->queue.front());
                self->queue.pop_front();
            }
            try {
                job.completion->value = job.run();
            } catch (const std::invalid_argument& e) {
                job.completion->error = e.what();
                job.completion->invalidArgument = true;
            } catch (const std::exception& e) {
                job.completion->error = e.what();
            } catch (...) {
                job.completion->error = "ECR17: unknown error";
            }
            // If the queue is gone (environment shutting down) the promise can't be settled;
            // the completion is then leaked on purpose: its JS handles must not be freed
            // here, off the main thread.
            self->post(new Delivery{std::move(job.completion), nullptr});
        }
        std::lock_guard<std::mutex> lock(self->postMutex);
        if (!self->released) {
            self->released = true;
            self->main.Release();
        }
    }

    // The main queue's finalizer (main thread): runs once it is fully released, or when the
    // environment tears down while the worker still holds it. From then on posts are
    // dropped; calling into a finalized thread-safe function aborts the process.
    static void onMainQueueFinalized(Napi::Env, std::shared_ptr<Worker>* worker, std::nullptr_t*) {
        {
            std::lock_guard<std::mutex> lock((*worker)->postMutex);
            (*worker)->released = true;
        }
        delete worker;
    }
};

void deliver(Napi::Env env, Napi::Function, std::nullptr_t*, Delivery* raw) {
    if (env == nullptr) {
        return;  // environment torn down: leak (see Worker::loop)
    }
    std::unique_ptr<Delivery> delivery(raw);
    Napi::HandleScope scope(env);
    if (delivery->event) {
        try {
            delivery->event(env);
        } catch (const Napi::Error& e) {
            // A throwing listener must not take the other deliveries down with it.
            e.ThrowAsJavaScriptException();
        }
        return;
    }
    Completion& completion = *delivery->completion;
    if (completion.value) {
        try {
            completion.deferred->Resolve(completion.value(env));
        } catch (const Napi::Error& e) {
            completion.deferred->Reject(e.Value());
        }
    } else {
        completion.deferred->Reject(toJsError(env, completion.error, completion.invalidArgument).Value());
    }
    if (--completion.worker->pending == 0) {
        completion.worker->main.Unref(env);
    }
}

// ---------- the client ----------

class NativeClient : public Napi::ObjectWrap<NativeClient> {
   public:
    static Napi::Function Define(Napi::Env env) {
        return DefineClass(env, "NativeClient",
                           {
                               InstanceMethod<&NativeClient::Connect>("connect"),
                               InstanceMethod<&NativeClient::Disconnect>("disconnect"),
                               InstanceMethod<&NativeClient::IsConnected>("isConnected"),
                               InstanceMethod<&NativeClient::Status>("status"),
                               InstanceMethod<&NativeClient::Pay>("pay"),
                               InstanceMethod<&NativeClient::PayExtended>("payExtended"),
                               InstanceMethod<&NativeClient::Reverse>("reverse"),
                               InstanceMethod<&NativeClient::PreAuth>("preAuth"),
                               InstanceMethod<&NativeClient::IncrementalAuth>("incrementalAuth"),
                               InstanceMethod<&NativeClient::PreAuthClosure>("preAuthClosure"),
                               InstanceMethod<&NativeClient::VerifyCard>("verifyCard"),
                               InstanceMethod<&NativeClient::CloseSession>("closeSession"),
                               InstanceMethod<&NativeClient::Totals>("totals"),
                               InstanceMethod<&NativeClient::SendLastResult>("sendLastResult"),
                               InstanceMethod<&NativeClient::EnableEcrPrinting>("enableEcrPrinting"),
                               InstanceMethod<&NativeClient::Reprint>("reprint"),
                               InstanceMethod<&NativeClient::Vas>("vas"),
                               InstanceMethod<&NativeClient::SetOnProgress>("setOnProgress"),
                               InstanceMethod<&NativeClient::SetOnReceiptLine>("setOnReceiptLine"),
                               InstanceMethod<&NativeClient::SetOnConnectionStateChange>("setOnConnectionStateChange"),
                               InstanceMethod<&NativeClient::Close>("close"),
                           });
    }

    explicit NativeClient(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NativeClient>(info) {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsObject()) {
            throw Napi::TypeError::New(env, "ECR17: NativeClient needs a config object");
        }
        try {
            client_ = std::make_shared<core::Ecr17Client>(std::make_shared<PlatformTransport>(),
                                                          toConfig(info[0].As<Napi::Object>()));
        } catch (const std::invalid_argument& e) {
            throw Napi::TypeError::New(env, e.what());
        }

        worker_ = std::make_shared<Worker>();
        worker_->main = MainQueue::New(env, "ecr17", 0, 1, static_cast<std::nullptr_t*>(nullptr),
                                       &Worker::onMainQueueFinalized, new std::shared_ptr<Worker>(worker_));
        worker_->main.Unref(env);

        // The core calls these on the command's thread or the transport's reader thread.
        std::weak_ptr<Worker> worker = worker_;
        std::weak_ptr<Listeners> listeners = listeners_;
        auto emitText = [worker, listeners](Napi::FunctionReference Listeners::*slot, std::string text) {
            if (auto w = worker.lock()) {
                w->postEvent([listeners, slot, text = std::move(text)](Napi::Env env) {
                    auto l = listeners.lock();
                    if (l && !((*l).*slot).IsEmpty()) ((*l).*slot).Call({Napi::String::New(env, text)});
                });
            }
        };
        client_->setOnProgress([emitText](const std::string& message) { emitText(&Listeners::progress, message); });
        client_->setOnReceiptLine([emitText](const std::string& line) { emitText(&Listeners::receipt, line); });
        client_->setOnConnectionStateChange([emitText](core::ConnectionState state) {
            emitText(&Listeners::state, stateName(state));
        });

        std::thread(&Worker::loop, worker_).detach();
    }

    ~NativeClient() override { shutdown(); }

   private:
    // Queues `run` on the worker thread and returns its promise.
    Napi::Value submit(const Napi::CallbackInfo& info, std::function<Materializer()> run) {
        Napi::Env env = info.Env();
        auto completion = std::make_unique<Completion>();
        completion->deferred = std::make_unique<Napi::Promise::Deferred>(env);
        Napi::Promise promise = completion->deferred->Promise();
        if (closed_) {
            completion->deferred->Reject(toJsError(env, "ECR17: the client is closed", false).Value());
            return promise;
        }
        completion->worker = worker_;
        completion->owner = std::make_shared<Napi::ObjectReference>(Napi::Persistent(info.This().As<Napi::Object>()));
        if (worker_->pending++ == 0) {
            worker_->main.Ref(env);
        }
        {
            std::lock_guard<std::mutex> lock(worker_->mutex);
            worker_->queue.push_back(Job{std::move(run), std::move(completion)});
        }
        worker_->changed.notify_one();
        return promise;
    }

    template <typename Response, typename Fn>
    Napi::Value command(const Napi::CallbackInfo& info, Fn fn) {
        auto client = client_;
        return submit(info, [client, fn]() -> Materializer {
            Response response = fn(*client);
            return [response = std::move(response)](Napi::Env env) -> Napi::Value { return toJs(env, response); };
        });
    }

    Napi::Value voidCommand(const Napi::CallbackInfo& info, std::function<void(core::Ecr17Client&)> fn) {
        auto client = client_;
        return submit(info, [client, fn]() -> Materializer {
            fn(*client);
            return [](Napi::Env env) -> Napi::Value { return env.Undefined(); };
        });
    }

    static Napi::Object requestArg(const Napi::CallbackInfo& info) {
        if (info.Length() < 1 || !info[0].IsObject()) {
            throw Napi::TypeError::New(info.Env(), "ECR17: expected a request object");
        }
        return info[0].As<Napi::Object>();
    }

    // Converts a request on the main thread, so a bad field throws synchronously.
    template <typename Fn>
    static auto convert(const Napi::CallbackInfo& info, Fn fn) {
        try {
            return fn(requestArg(info));
        } catch (const std::invalid_argument& e) {
            throw Napi::TypeError::New(info.Env(), e.what());
        }
    }

    Napi::Value Connect(const Napi::CallbackInfo& info) {
        return voidCommand(info, [](core::Ecr17Client& c) { c.connect(); });
    }

    // Synchronous: closes the socket (joins the reader thread, at most ~100 ms). A command
    // in flight then fails; a financial one is never re-sent (recover with sendLastResult).
    Napi::Value Disconnect(const Napi::CallbackInfo& info) {
        if (!closed_) client_->disconnect();
        return info.Env().Undefined();
    }

    // Synchronous: the transport's instant, write-free liveness probe.
    Napi::Value IsConnected(const Napi::CallbackInfo& info) {
        return Napi::Boolean::New(info.Env(), !closed_ && client_->isConnected());
    }

    Napi::Value Status(const Napi::CallbackInfo& info) {
        return command<core::StatusResponse>(info, [](core::Ecr17Client& c) { return c.status(); });
    }

    Napi::Value Pay(const Napi::CallbackInfo& info) {
        auto request = convert(info, toPayment);
        return command<core::PaymentResponse>(info, [request](core::Ecr17Client& c) { return c.pay(request); });
    }

    Napi::Value PayExtended(const Napi::CallbackInfo& info) {
        auto request = convert(info, toPayment);
        return command<core::PaymentResponse>(info, [request](core::Ecr17Client& c) { return c.payExtended(request); });
    }

    Napi::Value Reverse(const Napi::CallbackInfo& info) {
        auto request = convert(info, toReversal);
        return command<core::PaymentResponse>(info, [request](core::Ecr17Client& c) { return c.reverse(request); });
    }

    Napi::Value PreAuth(const Napi::CallbackInfo& info) {
        auto request = convert(info, toPayment);
        return command<core::PreAuthResponse>(info, [request](core::Ecr17Client& c) { return c.preAuth(request); });
    }

    Napi::Value IncrementalAuth(const Napi::CallbackInfo& info) {
        auto request = convert(info, toFollowUp);
        return command<core::PreAuthResponse>(info,
                                              [request](core::Ecr17Client& c) { return c.incrementalAuth(request); });
    }

    Napi::Value PreAuthClosure(const Napi::CallbackInfo& info) {
        auto request = convert(info, toFollowUp);
        return command<core::PaymentResponse>(info,
                                              [request](core::Ecr17Client& c) { return c.preAuthClosure(request); });
    }

    Napi::Value VerifyCard(const Napi::CallbackInfo& info) {
        auto request = convert(info, toVerification);
        return command<core::PaymentResponse>(info, [request](core::Ecr17Client& c) { return c.verifyCard(request); });
    }

    Napi::Value CloseSession(const Napi::CallbackInfo& info) {
        return command<core::CloseResponse>(info, [](core::Ecr17Client& c) { return c.closeSession(); });
    }

    Napi::Value Totals(const Napi::CallbackInfo& info) {
        return command<core::TotalsResponse>(info, [](core::Ecr17Client& c) { return c.totals(); });
    }

    Napi::Value SendLastResult(const Napi::CallbackInfo& info) {
        return command<core::PaymentResponse>(info, [](core::Ecr17Client& c) { return c.sendLastResult(); });
    }

    Napi::Value EnableEcrPrinting(const Napi::CallbackInfo& info) {
        const bool enabled = info.Length() > 0 && info[0].ToBoolean().Value();
        return voidCommand(info, [enabled](core::Ecr17Client& c) { c.enableEcrPrinting(enabled); });
    }

    Napi::Value Reprint(const Napi::CallbackInfo& info) {
        const bool toEcr = info.Length() > 0 && info[0].ToBoolean().Value();
        return voidCommand(info, [toEcr](core::Ecr17Client& c) { c.reprint(toEcr); });
    }

    Napi::Value Vas(const Napi::CallbackInfo& info) {
        if (info.Length() < 1 || !info[0].IsString()) {
            throw Napi::TypeError::New(info.Env(), "ECR17: vas needs an XML request string");
        }
        const std::string xml = info[0].As<Napi::String>().Utf8Value();
        return command<core::VasResponse>(info, [xml](core::Ecr17Client& c) { return c.vas(xml); });
    }

    static void setListener(Napi::FunctionReference& slot, const Napi::CallbackInfo& info) {
        if (info.Length() > 0 && info[0].IsFunction()) {
            slot = Napi::Persistent(info[0].As<Napi::Function>());
        } else {
            slot.Reset();
        }
    }

    Napi::Value SetOnProgress(const Napi::CallbackInfo& info) {
        if (!closed_) setListener(listeners_->progress, info);
        return info.Env().Undefined();
    }

    Napi::Value SetOnReceiptLine(const Napi::CallbackInfo& info) {
        if (!closed_) setListener(listeners_->receipt, info);
        return info.Env().Undefined();
    }

    Napi::Value SetOnConnectionStateChange(const Napi::CallbackInfo& info) {
        if (!closed_) setListener(listeners_->state, info);
        return info.Env().Undefined();
    }

    // Rejects the queued commands, closes the socket and stops the worker. A command
    // already on the wire fails like a dropped connection would: a financial one is
    // never re-sent, so its outcome must be recovered with sendLastResult() on a new
    // client.
    Napi::Value Close(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        std::deque<Job> abandoned;
        if (!closed_) {
            std::lock_guard<std::mutex> lock(worker_->mutex);
            abandoned.swap(worker_->queue);
        }
        for (Job& job : abandoned) {
            job.completion->deferred->Reject(toJsError(env, "ECR17: the client was closed", false).Value());
            if (--worker_->pending == 0) worker_->main.Unref(env);
        }
        shutdown();
        return env.Undefined();
    }

    void shutdown() {
        if (closed_) return;
        closed_ = true;
        // A closed client emits nothing, including the "disconnected" of the disconnect below
        // (at environment teardown the main queue may already be finalized).
        client_->setOnProgress(nullptr);
        client_->setOnReceiptLine(nullptr);
        client_->setOnConnectionStateChange(nullptr);
        listeners_->progress.Reset();
        listeners_->receipt.Reset();
        listeners_->state.Reset();
        try {
            client_->disconnect();
        } catch (...) {
            // The socket is gone either way.
        }
        {
            std::lock_guard<std::mutex> lock(worker_->mutex);
            worker_->stopping = true;
        }
        worker_->changed.notify_all();
    }

    std::shared_ptr<core::Ecr17Client> client_;
    std::shared_ptr<Worker> worker_;
    std::shared_ptr<Listeners> listeners_ = std::make_shared<Listeners>();
    bool closed_ = false;
};

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("NativeClient", NativeClient::Define(env));
    return exports;
}

}  // namespace

NODE_API_MODULE(ecr17, Init)
