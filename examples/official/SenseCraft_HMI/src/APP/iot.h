#pragma once

#include <functional>
#include "cJSON.h"
#include "app_config.h"
#include <Arduino.h>

using PublishFn = std::function<int(cJSON*)>;

class IotReportBuilder {
public:
    IotReportBuilder(const String& sessionId, PublishFn pub)
      : _publish(pub), _doc(nullptr), _data(nullptr), _states(nullptr), _descs(nullptr), _methods(nullptr), _hasMethodDesc(false)
    {
        _doc = cJSON_CreateObject();
        if (!_doc) return;

        cJSON_AddStringToObject(_doc, "version", Protocol_version);
        cJSON_AddStringToObject(_doc, "session_id", sessionId.c_str());
        cJSON_AddStringToObject(_doc, "type", "iot");
        cJSON_AddNumberToObject(_doc, "timestamp", (unsigned long long)millis());

        _data   = cJSON_CreateObject();
        cJSON_AddItemToObject(_doc, "data", _data);

        _states = cJSON_CreateArray();
        cJSON_AddItemToObject(_data, "states", _states);

        _descs  = cJSON_CreateArray();
        cJSON_AddItemToObject(_data, "descriptors", _descs);
    }

    ~IotReportBuilder() {
        if (_doc) {
            cJSON_Delete(_doc);
        }
    }

    IotReportBuilder& addState(const char* name, std::function<void(cJSON*)> filler)
    {
        if (!_states) return *this;

        cJSON* st = cJSON_CreateObject();
        cJSON_AddItemToArray(_states, st);

        cJSON_AddStringToObject(st, "name", name);
        
        cJSON* stateObj = cJSON_CreateObject();
        cJSON_AddItemToObject(st, "state", stateObj);

        filler(stateObj);
        return *this;
    }

    IotReportBuilder& addMethod(const char* moduleName,
                                const char* methodName,
                                const char* description,
                                std::function<void(cJSON*)> paramFiller)
    {
        if (!_descs) return *this;

        if (!_hasMethodDesc) {
            cJSON* desc = cJSON_CreateObject();
            cJSON_AddItemToArray(_descs, desc);
            
            _methods = cJSON_CreateObject();
            cJSON_AddItemToObject(desc, "methods", _methods);

            _hasMethodDesc = true;
        }
        
        if (!_methods) return *this;

        cJSON* module = cJSON_CreateObject();
        cJSON_AddItemToObject(_methods, moduleName, module);

        cJSON* method = cJSON_CreateObject();
        cJSON_AddItemToObject(module, methodName, method);

        cJSON_AddStringToObject(method, "description", description);
        
        cJSON* params = cJSON_CreateObject();
        cJSON_AddItemToObject(method, "parameters", params);

        paramFiller(params);
        return *this;
    }

    int send() {
        if (!_doc || !_publish) {
            return -1;
        }
        return _publish(_doc);
    }

private:
    PublishFn _publish;
    cJSON* _doc;
    cJSON* _data;
    cJSON* _states;
    cJSON* _descs;
    bool      _hasMethodDesc;
    cJSON* _methods;
};
