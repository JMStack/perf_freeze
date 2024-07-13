import 'package:flutter_test/flutter_test.dart';
import 'package:perf_freeze/perf_freeze.dart';
import 'package:perf_freeze/perf_freeze_platform_interface.dart';
import 'package:perf_freeze/perf_freeze_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockPerfFreezePlatform
    with MockPlatformInterfaceMixin
    implements PerfFreezePlatform {

  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

// void main() {
//   final PerfFreezePlatform initialPlatform = PerfFreezePlatform.instance;

//   test('$MethodChannelPerfFreeze is the default instance', () {
//     expect(initialPlatform, isInstanceOf<MethodChannelPerfFreeze>());
//   });

//   test('getPlatformVersion', () async {
//     PerfFreeze perfFreezePlugin = PerfFreeze();
//     MockPerfFreezePlatform fakePlatform = MockPerfFreezePlatform();
//     PerfFreezePlatform.instance = fakePlatform;

//     expect(await perfFreezePlugin.getPlatformVersion(), '42');
//   });
// }
