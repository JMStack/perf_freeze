import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'perf_freeze_platform_interface.dart';

/// An implementation of [PerfFreezePlatform] that uses method channels.
class MethodChannelPerfFreeze extends PerfFreezePlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('perf_freeze');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }
}
