import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'perf_freeze_method_channel.dart';

abstract class PerfFreezePlatform extends PlatformInterface {
  /// Constructs a PerfFreezePlatform.
  PerfFreezePlatform() : super(token: _token);

  static final Object _token = Object();

  static PerfFreezePlatform _instance = MethodChannelPerfFreeze();

  /// The default instance of [PerfFreezePlatform] to use.
  ///
  /// Defaults to [MethodChannelPerfFreeze].
  static PerfFreezePlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [PerfFreezePlatform] when
  /// they register themselves.
  static set instance(PerfFreezePlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
