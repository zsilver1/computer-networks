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
    fields {
        preamble: 64;
        num_valid: 32;
    }
}

header_type easy_route_port_t {
    fields {
        port: 8;
    }
}

header easy_route_t easy_route;
header easy_route_port_t er_port;

parser start {
    return select(current(0, 64)) {
        0: parse_easy_route;
        default: ingress;
    }
}

parser parse_easy_route {
    extract(easy_route);
    return select(easy_route.num_valid) {
        0: ingress;
        default: parse_port;
    }
}

parser parse_port {
    extract(er_port);
    return ingress;
}


action _drop() {
    drop();
}

action route() {
    modify_field(standard_metadata.egress_spec, er_port.port);
    remove_header(er_port);
    add_to_field(easy_route.num_valid, -1);
}

table tbl {
    reads {
        easy_route.num_valid: exact;
    }
    actions {
        _drop;
        route;
    }
    size:1;
}

control ingress {
    apply(tbl);
}

control egress {
    // leave empty
}
