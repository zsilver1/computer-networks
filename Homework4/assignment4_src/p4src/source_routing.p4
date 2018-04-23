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
header_type easy_route_t {
    bit<64>    preamble;
    bit<32>    num_valid;
    set<bit<8>> ports;
}

header easy_route_t easy_route;

parser start {
    // TODO
    return ingress;
}

parser my_parser

action _drop() {
    drop();
}

action route() {
    modify_field(standard_metadata.egress_spec, /* TODO: port field from your header */);
    // TODO: update your header
}

table tbl {
    reads {

    }
    actions {

    }
}

control ingress {
    // TODO
}

control egress {
    // leave empty
}
