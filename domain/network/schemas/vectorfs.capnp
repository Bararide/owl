@0xd34db33fc0d3701b;

using Cxx = import "/capnp/c++.capnp";

struct FileInfo {
    path @0 :Text;
    content @1 :Text;
    name @2 :Text;
    size @3 :UInt64;
    mode @4 :UInt32;
    created @5 :Bool;
}

struct SearchResult {
    path @0 :Text;
    score @1 :Float64;
}

struct SearchRequest {
    query @0 :Text;
    limit @1 :Int32 = 5;
}

struct SearchResponse {
    query @0 :Text;
    results @1 :List(SearchResult);
    count @2 :Int32;
}

struct FileRequest {
    path @0 :Text;
}

struct FileCreateRequest {
    path @0 :Text;
    content @1 :Text;
    name @2 :Text;
}

struct FileResponse {
    success @0 :Bool;
    file @1 :FileInfo;
    error @2 :Text;
}

struct StatusResponse {
    success @0 :Bool;
    message @1 :Text;
    error @2 :Text;
}

interface VectorFSService {
    createFile @0 (request :FileCreateRequest) -> (response :FileResponse);
    readFile @1 (request :FileRequest) -> (response :FileResponse);
    
    semanticSearch @2 (request :SearchRequest) -> (response :SearchResponse);
    
    rebuildIndex @3 () -> (response :StatusResponse);
    getStatus @4 () -> (response :StatusResponse);
    
    getStats @5 () -> (response :StatusResponse);
}