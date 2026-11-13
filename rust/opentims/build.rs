// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
fn main() {
    #[cfg(feature = "cpp")]
    {
        println!("cargo:rerun-if-changed=cpp/bridge.cpp");
        println!("cargo:rerun-if-changed=../../src/opentims++");
        cc::Build::new()
            .cpp(true)
            .std("c++20")
            .define("OPENTIMS_LINK_SQLITE_STATICALLY", None)
            .include("../../src/opentims++")
            .include("../../src/opentims++/sqlite")
            .file("cpp/bridge.cpp")
            .compile("opentims_bridge");
        if std::env::var("CARGO_CFG_TARGET_OS").unwrap() == "linux" {
            println!("cargo:rustc-link-lib=dl");
        }
    }
}
