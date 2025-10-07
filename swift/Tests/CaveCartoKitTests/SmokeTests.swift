import CCaveCarto
import CoreLocation
import Foundation
import XCTest

@testable import CaveCartoKit

struct TestStation: StationRepresentable {
  var type: CaveCartoKit.StationType
  var depth: Double
  var date: SurveyDate?
}

final class SmokeTests: XCTestCase {
  func testComputeRoundTrip() throws {

    let stationList: [TestStation] = [
      TestStation(
        type: .fixed(at: CLLocationCoordinate2D(latitude: 60, longitude: 10), parent: nil),
        depth: 0,
        date: nil),
      TestStation(
        type: .relative(toParent: 0, length: 10, azimuth: 90, linkedTo: nil, date: nil),
        depth: 0,
        date: nil),
    ]

    var stations = CCStations.makeStations(from: stationList)
    let datum = stations!.getDatum()
    XCTAssertNotNil(datum)
    let result = try stations!.getStationCoordinates(flags: .none)
    XCTAssertNotNil(result)
    stations = nil
  }
}
