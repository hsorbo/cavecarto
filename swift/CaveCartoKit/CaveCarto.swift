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

extension CCStation {
  public enum StationType {
    //case fixed(at: LatLon, linkedTo: Station?)
    //case relative(bearing: Double, distance: Double, linkedTo: Station)
    case fixed(at: CCLocation, parent: StationId?)
    case relative(to: CCDistance, parent: StationId, linkedTo: StationId?)
  }

  public enum StationType2 {
    //case fixed(at: LatLon, linkedTo: Station?)
    //case relative(bearing: Double, distance: Double, linkedTo: Station)
    case fixed(at: CCLocation, parent: UnsafeMutablePointer<CCStation>?)
    case relative(
      to: CCDistance, parent: UnsafeMutablePointer<CCStation>,
      linkedTo: UnsafeMutablePointer<CCStation>?)
  }

  public func id() -> Int {
    //assumes userData is set to id of station
    unsafeBitCast(self.user_data, to: Int.self)
  }

  public func type() -> StationType {
    //todo lookup parent/linked
    let parentId: StationId = 11
    return self.fixed
      ? StationType.fixed(at: self.location, parent: parentId)
      : StationType.relative(to: self.distance, parent: parentId, linkedTo: parentId)
  }

  public static func makeStation(
    type: StationType2,
    depth: Double,
    date: tm = tm(),
    userData: UnsafeMutableRawPointer? = nil
  ) -> CCStation {
    switch type {
    case .fixed(at: let location, let parent):
      let u = CCStation.__Unnamed_union___Anonymous_field4(location: location)
      return CCStation(
        depth: depth,
        fixed: true,
        parent: parent,
        closure: nil,
        u,
        date: date,
        user_data: userData
      )

    case .relative(to: let distance, let parent, linkedTo: let linked):
      let u = CCStation.__Unnamed_union___Anonymous_field4(distance: distance)
      return CCStation(
        depth: depth,
        fixed: false,
        parent: parent,
        closure: linked,
        u,
        date: date,
        user_data: userData
      )
    }
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
}
