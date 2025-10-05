import CCaveCarto
import CoreLocation

public struct TransformFlags: OptionSet {
  public let rawValue: UInt32
  public init(rawValue: UInt32) { self.rawValue = rawValue }

  public static let none = TransformFlags([])
  public static let closeLoops = TransformFlags(rawValue: CC_TRANSFORM_CLOSE_LOOPS.rawValue)
  public static let magNorth = TransformFlags(rawValue: CC_TRANSFORM_MAG_NORTH.rawValue)
}

public enum StationType<T> {
  case fixed(at: CLLocationCoordinate2D, parent: T?)
  case relative(toParent: T, length: Double, azimuth: Double, linkedTo: T?)
}

public protocol StationRepresentable {
  var id: Int32 { get }
  var type: StationType<Int32> { get }
  var depth: Double { get }
  var date: String? { get }
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

private func parseDateToTM(_ s: String?) -> tm {
  var t = tm()
  if let s, !s.isEmpty {
    s.withCString { c in _ = strptime(c, "%Y-%m-%d", &t) }
  }
  return t
}

public class CCStations {
  var count: Int

  var stations: UnsafeMutablePointer<CCStation>!
  var stationsBuffer: UnsafeMutableBufferPointer<CCStation> {
    UnsafeMutableBufferPointer(start: stations, count: count)
  }

  public init(stations: UnsafeMutablePointer<CCStation>, count: Int) {
    self.stations = stations
    self.count = count
  }

  deinit {
    free(self.stations)
  }

  public subscript(index: Int) -> CCStation {
    get {
      precondition(index >= 0 && index < count, "Index out of bounds")
      return stations[index]
    }
  }

  public func getDatum() -> CLLocationCoordinate2D? {
    for x in self.stationsBuffer {
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

    let stations = UnsafeMutablePointer<CCStation>.allocate(capacity: stationList.count)
    stations.initialize(repeating: CCStation(), count: stationList.count)

    for (i, station) in stationList.enumerated() {
      switch station.type {
      case .fixed(let at, let parent):
        cc_station_init_fixed(
          &stations[i],
          parent != nil ? stations.advanced(by: Int(parent!)) : nil,
          CCLocation(lon: at.longitude, lat: at.latitude),
          station.depth,
          parseDateToTM(station.date),
          nil)
      case .relative(toParent: let parent, let length, let azimuth, let linkedTo):
        cc_station_init_relative(
          &stations[i],
          stations.advanced(by: Int(parent)),
          linkedTo != nil ? stations.advanced(by: Int(linkedTo!)) : nil,
          CCaveCarto.CCDistance(azimuth: azimuth, length: length),
          station.depth,
          parseDateToTM(station.date),
          nil)
      }
    }

    return CCStations(stations: stations, count: stationList.count)
  }

  private func stationXy(
    datum: CCLocation,
    flags: TransformFlags
  ) throws -> UnsafeMutablePointer<CCPoint> {
    let points = UnsafeMutablePointer<CCPoint>.allocate(capacity: count)
    try! withUnsafeTemporaryAllocation(of: CCLocation.self, capacity: 1) { ptr in
      ptr.initialize(repeating: datum)
      let result = cc_compute_station_xy(
        stations, count, ptr.baseAddress!, points, CCTransformFlags(flags.rawValue))
      if let error = CaveCartoError.raisy(result: result) {
        free(points)
        throw error
      }
    }
    return points
  }

  public func getStationCoordinates(flags: TransformFlags) throws -> [SIMD3<Float>] {
    let datum = self.getDatum()!
    let points = try self.stationXy(
      datum: CCLocation(lon: datum.longitude, lat: datum.longitude),
      flags: flags
    )
    defer {
      points.deallocate()
    }

    var v: [SIMD3<Float>] = []
    v.reserveCapacity(self.count)
    for i in 0..<self.count {
      let station = self.stations[i]
      v.append(SIMD3<Float>(Float(points[i].x), Float(points[i].y), Float(-station.depth)))
    }
    return v
  }
}
