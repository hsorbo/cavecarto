import CCaveCarto
import CoreLocation

public typealias SurveyDate = CCaveCarto.CCDate

public struct TransformFlags: OptionSet {
  public let rawValue: UInt32
  public init(rawValue: UInt32) { self.rawValue = rawValue }

  public static let none = TransformFlags([])
  public static let closeLoops = TransformFlags(rawValue: CC_TRANSFORM_CLOSE_LOOPS.rawValue)
  public static let magNorth = TransformFlags(rawValue: CC_TRANSFORM_MAG_NORTH.rawValue)
  public static let magNorthFast = TransformFlags(rawValue: CC_TRANSFORM_MAG_NORTH_FAST.rawValue)
}

public enum StationType {
  case fixed(at: CLLocationCoordinate2D, parent: Int?)
  case relative(toParent: Int, length: Double, azimuth: Double, linkedTo: Int?, date: SurveyDate?)
}

public protocol StationRepresentable {
  var type: StationType { get }
  var depth: Double { get }
}

extension SurveyDate {
  public func zero() -> SurveyDate { return SurveyDate(day: 0, month: 0, year: 0) }

  public static func makeDate(year: UInt16, month: UInt8, day: UInt8) -> SurveyDate {
    return cc_make_date(year, month, day)
  }
}

public enum CaveCartoError: Error {
  case unknownError
  case stationNullError
  case cycleDetectedError
  case relativeStationMissingParentError
  case noDatumFoundError

  static func raisy(result: CCComputeResult) -> CaveCartoError? {
    switch result {
    case CCStation_SUCCESS:
      return nil
    case CCStation_NULL:
      return stationNullError
    case CCStation_NO_DATUM:
      return noDatumFoundError
    case CCStation_CYCLE_DETECTED:
      return cycleDetectedError
    case CCStation_RELATIVE_MISSING_PARENT:
      return relativeStationMissingParentError
    default:
      return unknownError
    }
  }
}

public class CCStations {
  public private(set) var stations: [CCStation]
  public var count: Int { stations.count }

  public init(stations: [CCStation]) {
    self.stations = stations
  }

  public func getDatum() -> CLLocationCoordinate2D? {
    for x in stations {
      if x.fixed {
        return CLLocationCoordinate2D(latitude: x.location.lat, longitude: x.location.lon)
      }
    }
    return nil
  }

  public static func makeStations(from stationList: [StationRepresentable]) -> CCStations? {
    guard !stationList.isEmpty else {
      return nil
    }

    var stations = Array(repeating: CCStation(), count: stationList.count)

    for (i, station) in stationList.enumerated() {
      switch station.type {
      case .fixed(let at, let parent):
        cc_station_init_fixed(
          &stations[i],
          parent != nil ? parent! : -1,
          at.latitude,
          at.longitude,
          station.depth)
      case .relative(toParent: let parent, let length, let azimuth, let linkedTo, let date):
        cc_station_init_relative(
          &stations[i],
          parent,
          linkedTo != nil ? linkedTo! : -1,
          azimuth,
          length,
          station.depth,
          date ?? CCDate(day: 0, month: 0, year: 0))
      }
    }

    return CCStations(stations: stations)
  }

  private func stationXy(
    datum: CCLocation,
    flags: TransformFlags
  ) throws -> [CCPoint] {
    var points = Array(repeating: CCPoint(), count: count)
    let n = points.count  // capture before closures
    var d = datum  // keep a var we can take & of
    let cflags = CCTransformFlags(flags.rawValue)

    try stations.withUnsafeBufferPointer { sBuf in
      try points.withUnsafeMutableBufferPointer { pBuf in
        let result = cc_compute_station_xy(
          UnsafeMutablePointer(mutating: sBuf.baseAddress),  // OK to cast for C API
          n,
          &d,
          pBuf.baseAddress!,
          cflags
        )
        if let error = CaveCartoError.raisy(result: result) {
          throw error
        }
      }
    }
    return points
  }

  public func getStationCoordinates(flags: TransformFlags) throws -> [SIMD3<Float>] {
    guard let datum = self.getDatum() else { throw CaveCartoError.noDatumFoundError }

    let points = try self.stationXy(
      datum: CCLocation(lon: datum.longitude, lat: datum.latitude),
      flags: flags
    )

    var v: [SIMD3<Float>] = []
    v.reserveCapacity(count)
    for i in 0..<count {
      let station = stations[i]
      v.append(SIMD3<Float>(Float(points[i].x), Float(points[i].y), Float(-station.depth)))
    }
    return v
  }
}
