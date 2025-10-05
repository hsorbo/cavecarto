import CCaveCarto
import CoreLocation
import XCTest

@testable import CaveCartoKit

struct TestStation: StationRepresentable {
  var id: Int32
  var type: CaveCartoKit.StationType<Int32>
  var depth: Double
  var date: String?
}

final class SmokeTests: XCTestCase {
  func testComputeRoundTrip() throws {

    let stationList: [TestStation] = [
      TestStation(
        id: 1,
        type: .fixed(at: CLLocationCoordinate2D(latitude: 60, longitude: 10), parent: nil),
        depth: 0,
        date: ""),
      TestStation(
        id: 2,
        type: .relative(toParent: 0, length: 10, azimuth: 90, linkedTo: nil),
        depth: 0,
        date: ""),
    ]

    var stations = CCStations.makeStations(from: stationList)
    let datum = stations!.getDatum()
    XCTAssertNotNil(datum)
    let result = try stations!.getStationCoordinates(flags: .none)
    XCTAssertNotNil(result)
    stations = nil
  }
}
