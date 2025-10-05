import CCaveCarto
import CoreLocation

public typealias StationId = Int32

public typealias CCPoint = CCaveCarto.CCPoint
public typealias CCLocation = CCaveCarto.CCLocation
public typealias CCDistance = CCaveCarto.CCDistance
public typealias CCStation = CCaveCarto.CCStation
public typealias CCTransformFlags = CCaveCarto.CCTransformFlags
public typealias CCComputeResult = CCaveCarto.CCComputeResult

public struct TransformFlags: OptionSet {
  public let rawValue: UInt32
  public init(rawValue: UInt32) { self.rawValue = rawValue }

  public static let none = TransformFlags([])
  public static let closeLoops = TransformFlags(rawValue: CC_TRANSFORM_CLOSE_LOOPS.rawValue)
  public static let magNorth = TransformFlags(rawValue: CC_TRANSFORM_MAG_NORTH.rawValue)
}

extension CCLocation {
  public var coordinate: CLLocationCoordinate2D {
    CLLocationCoordinate2D(latitude: lat, longitude: lon)
  }
}

public enum StationType<T> {
  case fixed(at: CCLocation, parent: T?)
  case relative(to: CCDistance, parent: T, linkedTo: T?)
}

public protocol StationP {
  var id: Int32 {get}
  var type: StationType<StationId> {get}
  var depth: Double {get}
  var date: String? {get}
}

extension CCStation {

  public func id() -> Int {
    //assumes userData is set to id of station
    unsafeBitCast(self.user_data, to: Int.self)
  }

  public func type() -> StationType<StationId> {
    //todo lookup parent/linked
    let parentId: StationId = 11
    return self.fixed
      ? StationType.fixed(at: self.location, parent: parentId)
      : StationType.relative(to: self.distance, parent: parentId, linkedTo: parentId)
  }

  public static func makeStation(
    type: StationType<UnsafeMutablePointer<CCStation>>,
    depth: Double,
    date: tm = tm(),
    userData: UnsafeMutableRawPointer? = nil
  ) -> CCStation {
    var s = CCStation()
    switch type {
    case .fixed(at: let location, let parent):
      cc_station_init_fixed(&s, parent, location, depth, date, userData)
    case .relative(to: let distance, let parent, linkedTo: let linked):
      cc_station_init_relative(&s, parent, linked, distance, depth, date, userData)
    }
    return s;
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

public class Compute {
  public static func stationXy(
    stations: UnsafeMutablePointer<CCStation>,
    count: Int,
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
}


private func parseDateToTM(_ s: String?) -> tm {
  var t = tm()
  if let s, !s.isEmpty {
    s.withCString { c in _ = strptime(c, "%Y-%m-%d", &t) }
  }
  return t
}

public class CCStations: RandomAccessCollection {
  public typealias Index = Int
  public typealias Element = CCStation

  public var count: Int

  //todo: hide stations/ stationsBuffers
  public var stations: UnsafeMutablePointer<CCStation>!
  public var stationsBuffer: UnsafeMutableBufferPointer<CCStation> {
    UnsafeMutableBufferPointer(start: stations, count: count)
  }

  // Required by Collection
  public var startIndex: Int { 0 }
  public var endIndex: Int { count }

  public subscript(index: Int) -> CCStation {
    get {
      precondition(index >= 0 && index < count, "Index out of bounds")
      return stations[index]
    }
  }

  public init(stations: UnsafeMutablePointer<CCStation>, count: Int) {
    self.stations = stations
    self.count = count
  }

  deinit {
    free(self.stations)
  }

  public func getDatum() -> CCLocation? {
    for x in self.stationsBuffer {
      if x.fixed { return x.location }
    }
    return nil
  }

  public func buildCCStations(from s: [StationP]) -> CCStations? {
     guard !s.isEmpty else {
      return nil
    }

    let count = s.count

    // Allocate a zeroed/known-initialized buffer
    let stations = UnsafeMutablePointer<CCStation>.allocate(capacity: count)
    let empty = CCStation.makeStation(
      type: .fixed(at: CCLocation(lon: 0.0, lat: 0.0), parent: nil),
      depth: 0,
      date: tm(),
      userData: nil
    )
    stations.initialize(repeating: empty, count: count)

    // id → index map for relationship fixups
    var indexById: [Int32: Int] = [:]
    indexById.reserveCapacity(count)
    for (i, dto) in s.enumerated() { indexById[dto.id] = i }

    // First pass: fill scalars + union + date + user_data
    for (i, dto) in s.enumerated() {
      var s = stations[i]
      s.depth = dto.depth

      switch dto.type {
      case .fixed(let at, _):
        s.fixed = true
        s.location = at

      case .relative(let to, _, _):
        s.fixed = false
        s.distance = to
      }
      s.date = parseDateToTM(dto.date)
      s.parent = nil
      s.closure = nil

      // Preserve C behavior: stash integer id in user_data
      s.user_data = UnsafeMutableRawPointer(bitPattern: UInt(bitPattern: Int(dto.id)))

      stations[i] = s
    }

    // Second pass: fix pointers for parent/closure
    for (i, dto) in s.enumerated() {
      
      switch dto.type {
      case .fixed(_, let parent):
        if let pid = parent, let pidx = indexById[pid] {
          stations[i].parent = stations.advanced(by: pidx)
        }

      case .relative(_, let parent, let linkedTo):
        if let pidx = indexById[parent] {
          stations[i].parent = stations.advanced(by: pidx)
        }
        if let lid = linkedTo, let lidx = indexById[lid] {
          stations[i].closure = stations.advanced(by: lidx)
        }
      }
    }

    return CCStations(stations: stations, count: count)
  }
}
