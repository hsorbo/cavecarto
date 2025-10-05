import CCaveCarto
import CoreLocation
import XCTest
import Foundation

@testable import CaveCartoKit

struct TestStation: StationRepresentable {
  var type: CaveCartoKit.StationType
  var depth: Double
  var date: Date?
}

final class SmokeTests: XCTestCase {
  func testComputeRoundTrip() throws {

    let stationList: [TestStation] = [
      TestStation(
        type: .fixed(at: CLLocationCoordinate2D(latitude: 60, longitude: 10), parent: nil),
        depth: 0,
        date: Date.distantPast),
      TestStation(
        type: .relative(toParent: 0, length: 10, azimuth: 90, linkedTo: nil),
        depth: 0,
        date: Date.distantPast),
    ]

    var stations = CCStations.makeStations(from: stationList)
    let datum = stations!.getDatum()
    XCTAssertNotNil(datum)
    let result = try stations!.getStationCoordinates(flags: .none)
    XCTAssertNotNil(result)
    stations = nil
  }
}
