// swift-tools-version: 5.9
import PackageDescription

let package = Package(
  name: "cavecarto",
  platforms: [
    .iOS(.v14), .macOS(.v12)
  ],
  products: [
    .library(name: "CaveCartoKit", targets: ["CaveCartoKit"])
  ],
  targets: [
    .target(
      name: "CCaveCarto",
      path: ".",
      sources: ["src"],
      publicHeadersPath: "include",
      cSettings: [
        .headerSearchPath("src")
      ],
      linkerSettings: [
        .linkedLibrary("m", .when(platforms: [.linux]))
      ]
    ),

    .target(
      name: "CaveCartoKit",
      dependencies: ["CCaveCarto"],
      path: "swift/CaveCartoKit",
      resources: [
      ]
    ),

    .testTarget(
      name: "CaveCartoKitTests",
      dependencies: ["CaveCartoKit"],
      path: "swift/Tests/CaveCartoKitTests"
    )
  ]
)