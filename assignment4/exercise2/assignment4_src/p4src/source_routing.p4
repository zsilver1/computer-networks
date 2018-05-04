/*
Copyright 2013-present Barefoot Networks, Inc. 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// HEADERS

header_type kv_store_t {
    fields {
        preamble: 64;
        num_valid: 32;
        port: 8;
        mytype: 8;
        key: 32;
        value: 32;
    }
}

register key_value_store {
    width : 32;
    instance_count : 1000;
}

header kv_store_t key_value;

parser start {
    return select(current(0, 64)) {
        1: parse_key_value;
        default: ingress;
    }
}

parser parse_key_value {
    extract(key_value);
    return ingress;
}


action _drop() {
    drop();
}

action get_request() {
    modify_field(standard_metadata.egress_spec, standard_metadata.ingress_port);
    register_read(key_value.value, key_value_store, key_value.key);
    modify_field(key_value.mytype, 2);
}

action put_request() {
    modify_field(standard_metadata.egress_spec, standard_metadata.ingress_port);
    register_write(key_value_store, key_value.key, key_value.value);
    modify_field(key_value.mytype, 3);
}

table tbl {
    reads {
        key_value.mytype: exact;
    }
    actions {
        _drop;
        put_request;
        get_request;
    }
}

control ingress {
    apply(tbl);
}

control egress {
    // leave empty
}
