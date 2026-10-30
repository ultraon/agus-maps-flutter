import 'dart:ui';

/// What a two-finger gesture on the map does.
enum TwoFingerGesture {
  /// The fingers have not moved far enough to tell.
  undecided,

  /// Both fingers move up or down together: the map tilts.
  tilt,

  /// Any other move: the engine zooms and rotates the map.
  pinch,
}

/// Tells a two-finger tilt from a pinch or a rotation, from where the two
/// fingers started and where they are now, in logical pixels.
///
/// A tilt needs two fingers roughly side by side that both move up, or both
/// move down, mostly vertically. Real fingers drift, so the distance between
/// them may change by up to [maxSpreadChange]. A clear pinch, a rotation or a
/// sideways move decides at once; an unclear move waits until a finger has
/// moved [decideDistance]. The caller keeps the choice for the rest of the
/// gesture.
class TwoFingerTiltDetector {
  /// A finger must move this far before the gesture can be decided.
  final double slop;

  /// For a tilt, each finger moves at least this many times more vertically
  /// than horizontally.
  final double verticalRatio;

  /// A larger change of the distance between the fingers is a pinch.
  final double maxSpreadChange;

  /// The fingers are side by side when the line between them is at most
  /// this many times steeper than flat (1.73 is 60 degrees).
  final double maxFingerSlope;

  /// An unclear gesture becomes a pinch once a finger moved this far.
  final double decideDistance;

  const TwoFingerTiltDetector({
    this.slop = 12,
    this.verticalRatio = 1.5,
    this.maxSpreadChange = 24,
    this.maxFingerSlope = 1.73,
    this.decideDistance = 36,
  });

  TwoFingerGesture classify({
    required Offset start1,
    required Offset start2,
    required Offset now1,
    required Offset now2,
  }) {
    final move1 = now1 - start1;
    final move2 = now2 - start2;
    final longest =
        move1.distance > move2.distance ? move1.distance : move2.distance;
    if (longest < slop) return TwoFingerGesture.undecided;
    final between = start2 - start1;
    final spreadChange = ((now2 - now1).distance - between.distance).abs();
    final sideBySide = between.dy.abs() <= maxFingerSlope * between.dx.abs();
    if (!sideBySide || spreadChange > maxSpreadChange) {
      return TwoFingerGesture.pinch;
    }
    final opposite = move1.dy.sign != move2.dy.sign &&
        move1.dy.abs() >= slop / 2 &&
        move2.dy.abs() >= slop / 2;
    final sideways = (move1.distance >= slop && !_isVertical(move1)) ||
        (move2.distance >= slop && !_isVertical(move2));
    if (opposite || sideways) return TwoFingerGesture.pinch;
    final bothRise = _isVertical(move1) &&
        _isVertical(move2) &&
        move1.dy.sign == move2.dy.sign &&
        move1.dy.abs() >= slop / 2 &&
        move2.dy.abs() >= slop / 2;
    if (bothRise) return TwoFingerGesture.tilt;
    return longest >= decideDistance
        ? TwoFingerGesture.pinch
        : TwoFingerGesture.undecided;
  }

  bool _isVertical(Offset move) =>
      move.dy != 0 && move.dy.abs() >= verticalRatio * move.dx.abs();
}

/// The tilt change for each logical pixel of a two-finger drag. A drag over
/// about a third of a phone screen goes from flat to the largest tilt.
const double kTiltDegreesPerPixel = 0.25;

/// The tilt in degrees after the fingers moved [dragDy] logical pixels down
/// from where the tilt began at [startAngle]. A drag up tilts the map more,
/// as in Google Maps. The result stays between 0 (flat) and [maxAngle].
double tiltAngleForDrag({
  required double startAngle,
  required double dragDy,
  required double maxAngle,
  double degreesPerPixel = kTiltDegreesPerPixel,
}) {
  final angle = startAngle - dragDy * degreesPerPixel;
  if (angle < 0) return 0;
  return angle > maxAngle ? maxAngle : angle;
}
