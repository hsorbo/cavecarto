import CCaveCarto
import XCTest

@testable import CaveCartoKit

final class SmokeTests: XCTestCase {
  func testComputeRoundTrip() throws {

    let stationsPtr = UnsafeMutablePointer<CCStation>.allocate(capacity: 2)
    stationsPtr.initialize(repeating: CCStation(), count: 2)

    stationsPtr[0] = CCStation.makeStation(
      type: StationType.fixed(
        at: CCLocation(lon: 10, lat: 60),
        parent: nil),
      depth: 0,
      date: tm(),
      userData: UnsafeMutableRawPointer(bitPattern: 1)
    )

    stationsPtr[1] = CCStation.makeStation(
      type:
        StationType.relative(
          to: CCDistance(azimuth: 90, length: 10),
          parent: stationsPtr + 0, linkedTo: nil,
        ),
      depth: 2.5,
      date: tm(),
      userData: UnsafeMutableRawPointer(bitPattern: 2))

    var stations: CCStations? = CCStations(stations: stationsPtr, count: 2)
    XCTAssertNotNil(stations?.getDatum())
    stations = nil
  }
}
