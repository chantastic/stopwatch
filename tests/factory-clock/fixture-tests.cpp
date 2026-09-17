#undef time
#undef settimeofday

struct PhoneResult { bool ok = false; badge::ClockSnapshot snapshot; std::string error; };
PhoneResult phone(int64_t value, int offset) {
    PhoneResult result; std::atomic<bool> done{false};
    std::thread worker([&] { result.ok = badge_clock::setFromPhone(value, offset, result.snapshot, result.error); done = true; });
    while (!done) { badge_clock::poll(); std::this_thread::yield(); }
    worker.join(); return result;
}

// Record the actual production response builder's cJSON field writes. We test
// the snapshot-to-response contract; this is not an HTTP/socket simulation.
using JsonValue = std::variant<bool, double, std::string>;
struct cJSON { double valuedouble = 0; std::map<std::string, JsonValue> fields; };
using ClockSnapshot = badge::ClockSnapshot;
using httpd_req_t = int;
static cJSON serialized;
static int response_status = 0;
cJSON* cJSON_CreateObject() { return new cJSON; }
void cJSON_Delete(cJSON* value) { delete value; }
void cJSON_AddBoolToObject(cJSON* value, const char* key, bool item) { value->fields[key] = item; }
void cJSON_AddNumberToObject(cJSON* value, const char* key, double item) { value->fields[key] = item; }
void cJSON_AddStringToObject(cJSON* value, const char* key, const char* item) { value->fields[key] = std::string(item); }
int json_reply(httpd_req_t*, int status, cJSON* value) { serialized = *value; response_status = status; return 0; }
int response(int64_t requested_epoch, int requested_offset, const PhoneResult& returned) {
    cJSON epoch_value, offset_value; epoch_value.valuedouble = requested_epoch; offset_value.valuedouble = requested_offset;
    auto* epoch = &epoch_value; auto* offset = &offset_value; httpd_req_t* req = nullptr;
    badge::PhoneClockSync sync_clock = [&](int64_t value, int zone, badge::ClockSnapshot& snapshot, std::string& error) {
        assert(value == requested_epoch && zone == requested_offset);
        snapshot = returned.snapshot; error = returned.error; return returned.ok;
    };
    // PRODUCTION_CLOCK_RESPONSE_HERE
}

int main() {
    badge_clock::init();
    assert(badge_clock::valid() && std::string(badge_clock::source()) == "rtc");
    std::string error;
    assert(badge_clock::set(1789670400, -420, "computer", error) && writes == 1);
    assert(badge_clock::set(1789670460, -420, "computer", error) && writes == 1);
    assert(!badge_clock::set(1789670460, 841, "computer", error) && error == "invalid_value");
    assert(!badge_clock::set(badge_clock::MaxEpoch, 0, "computer", error));
    assert(badge_clock::set(badge_clock::MinEpoch, -420, "computer", error));
    assert(badge_clock::dateText() == "2023-12-31 17:00");
    assert(badge_clock::validNonce("0123456789abcdef") && !badge_clock::validNonce("invalid"));

    // Actual readback advances one second: the response must not echo input.
    write_adjustment = 1;
    auto actual = phone(1789670600, 345);
    assert(actual.ok && actual.error.empty() && actual.snapshot.valid);
    assert(actual.snapshot.epoch == 1789670601 && actual.snapshot.rtc_epoch == 1789670601);
    assert(actual.snapshot.offset_minutes == 345 && actual.snapshot.source == "phone");
    response(1789670600, 345, actual);
    assert(response_status == 200 && std::get<bool>(serialized.fields.at("ok")) && std::get<bool>(serialized.fields.at("valid")));
    assert(std::get<double>(serialized.fields.at("epoch")) == 1789670601);
    assert(std::get<double>(serialized.fields.at("rtc_epoch")) == 1789670601);
    assert(std::get<double>(serialized.fields.at("offset_minutes")) == 345);
    assert(std::get<std::string>(serialized.fields.at("source")) == "phone");
    write_adjustment = 0;

    // A second HTTP caller cannot replace an outstanding phone request.
    PhoneResult queued; std::atomic<bool> done{false};
    std::thread worker([&] { queued.ok = badge_clock::setFromPhone(1789670700, -60, queued.snapshot, queued.error); done = true; });
    for (;;) { std::lock_guard<std::mutex> lock(badge_clock::requestMutex); if (badge_clock::pending) break; }
    badge::ClockSnapshot busy;
    assert(!badge_clock::setFromPhone(1789670800, 0, busy, error) && error == "clock_busy" && !busy.valid);
    while (!done) { badge_clock::poll(); std::this_thread::yield(); }
    worker.join(); assert(queued.ok && queued.snapshot.epoch == 1789670700 && queued.snapshot.offset_minutes == -60);

    // Failure during final verification must not produce a successful response.
    final_read_fails = true;
    auto failed = phone(1789670900, 0);
    assert(!failed.ok && !failed.snapshot.valid && failed.error == "clock_verify_failed");
    assert(failed.snapshot.rtc_epoch == 0 && failed.snapshot.source == "unset" && !badge_clock::valid());
    response(1789670900, 0, failed);
    assert(response_status == 400 && !std::get<bool>(serialized.fields.at("ok")) && !std::get<bool>(serialized.fields.at("valid")));
    assert(std::get<double>(serialized.fields.at("rtc_epoch")) == 0);
    assert(std::get<std::string>(serialized.fields.at("error")) == "clock_verify_failed");
    final_read_fails = false;

    final_read_skew = 5;
    failed = phone(1789671000, 0);
    assert(!failed.ok && failed.error == "clock_verify_failed" && failed.snapshot.rtc_epoch == 1789671005);
    final_read_skew = 0;
    rtc_write_ok = false; failed = phone(1789671100, 0);
    assert(!failed.ok && failed.error == "rtc_write_failed" && !failed.snapshot.valid);
    rtc_write_ok = true;
    nvs_ok = false; failed = phone(1789671200, 420);
    assert(!failed.ok && failed.error == "offset_store_failed" && !failed.snapshot.valid);
    nvs_ok = true;
    system_ok = false; failed = phone(1789671300, 420);
    assert(!failed.ok && failed.error == "system_clock_failed" && !failed.snapshot.valid);
    system_ok = true;
    auto recovered = phone(1789671400, -840);
    assert(recovered.ok && recovered.snapshot.valid && recovered.snapshot.offset_minutes == -840);
    puts("Native clock: RTC retention, UTC bounds/rollover, offset persistence, worker queue/busy, verified response fields and injected readback failures passed");
}
