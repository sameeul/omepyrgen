#include "utilities.h"

tensorstore::Spec GetOmeTiffSpecToRead(const std::string& filename){
    return tensorstore::Spec::FromJson({{"driver", "ometiff"},

                            {"kvstore", {{"driver", "tiled_tiff"},
                                         {"path", filename}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 8}}},
                              {"file_io_concurrency", {{"limit", 8}}},
                            }},
                            }).value();
}

tensorstore::Spec GetZarrSpecToWrite(   const std::string& filename, 
                                        std::vector<std::int64_t>& image_shape, 
                                        std::vector<std::int64_t>& chunk_shape,
                                        std::string& dtype){
    return tensorstore::Spec::FromJson({{"driver", "zarr"},
                            {"kvstore", {{"driver", "file"},
                                         {"path", filename}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 8}}},
                              {"file_io_concurrency", {{"limit", 8}}},
                            }},
                            {"metadata", {
                                          {"zarr_format", 2},
                                          {"shape", image_shape},
                                          {"chunks", chunk_shape},
                                          {"dtype", dtype},
                                          },
                            }}).value();
}

tensorstore::Spec GetZarrSpecToRead(const std::string& filename){
    return tensorstore::Spec::FromJson({{"driver", "zarr"},
                            {"kvstore", {{"driver", "file"},
                                         {"path", filename}}
                            }
                            }).value();
}

tensorstore::Spec GetPCNSpecToWrite(const std::string& filename, 
                                    const std::string& scale_key,
                                    const std::vector<std::int64_t>& image_shape, 
                                    const std::vector<std::int64_t>& chunk_shape,
                                    std::string_view dtype){
    return tensorstore::Spec::FromJson({{"driver", "neuroglancer_precomputed"},
                            {"kvstore", {{"driver", "file"},
                                         {"path", filename}}
                            },
                            {"context", {
                              {"cache_pool", {{"total_bytes_limit", 1000000000}}},
                              {"data_copy_concurrency", {{"limit", 8}}},
                              {"file_io_concurrency", {{"limit", 8}}},
                            }},
                            {"multiscale_metadata", {
                                          {"data_type", dtype},
                                          {"num_channels", 1},
                                          {"type", "image"},
                            }},
                            {"scale_metadata", {
                                          {"encoding", "raw"},
                                          {"key", scale_key},
                                          {"size", image_shape},
                                          {"chunk_size", chunk_shape},
                                          },
                            }}).value();
}
