// You have generated a new plugin project without specifying the `--platforms`
// flag. A plugin project with no platform support was generated. To add a
// platform, run `flutter create -t plugin --platforms <platforms> .` under the
// same directory. You can also find a detailed instruction on how to add
// platforms in the `pubspec.yaml` at
// https://flutter.dev/docs/development/packages-and-plugins/developing-packages#plugin-platforms.

import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:isolate';
import 'package:ffi/ffi.dart';

import 'generated_bindings.dart';
import 'dart:ffi';

import 'package:path/path.dart' show basename;

const int ping = 0x55;
const int pong = 0xAA;

class PerfFreezeMonitor {
  static PerfFreezeMonitor? _instance;
  PerfFreezeMonitor._();
  factory PerfFreezeMonitor() => _instance ??= PerfFreezeMonitor._();

  Isolate? _workerIsolate;

  ReceivePort? _mainReceivePort;


  void startMonitor({Duration sampling = const Duration(milliseconds: 25)}) async {
    if (_mainReceivePort != null || _workerIsolate != null) return;

    _mainReceivePort = ReceivePort();

    _workerIsolate = await Isolate.spawn(_workerIsolateFun, _mainReceivePort!.sendPort);

    SendPort? workerSendPort;

    _mainReceivePort!.listen((data){
      if (data is SendPort) {
        workerSendPort  = data;
        return;
      }
      if (data is int && identical(data, ping)) {
        workerSendPort?.send(pong);
        return;
      }
    });
  }

  void stopMonitor() {
    _mainReceivePort?.close();
    _mainReceivePort = null;
    _workerIsolate?.kill(priority: Isolate.immediate);
    _workerIsolate = null;
  }

}

void _workerIsolateFun(SendPort workerSendPort)  async {
  final workerReceivePort = ReceivePort();
  workerSendPort.send(workerReceivePort.sendPort);

  late Completer syncCompleter; 

  workerReceivePort.listen((p) {
    if (p is int && identical(p, pong)) {
      syncCompleter.complete();
    }
  });

  NativePerfFreezeMonitor? monitor;
  
  while (true) {
    syncCompleter = Completer();
    workerSendPort.send(ping);
    await Future.delayed(const Duration(milliseconds: 25));
    if (!syncCompleter.isCompleted) {
      const stackDepth = 32;

      Pointer<TraceInfo> pointer = calloc<TraceInfo>(stackDepth);

      (monitor ??= NativePerfFreezeMonitor(DynamicLibrary.process())).uiThreadStacktrace(pointer, stackDepth);

      final stacktrace = _toJsonFormat(pointer, stackDepth);

      // 写文件或上报服务端
      final file = File('xxx/xx/stacktrace.json');
      file.writeAsStringSync(stacktrace);

      calloc.free(pointer);
      await syncCompleter.future;
    }
  }
}

String _toJsonFormat(Pointer<TraceInfo> pointer, int length) {
  final list = List<Map<String, dynamic>>.empty(growable: true);
  for (var i = 0; i < length; i++) {
    final dlInfo = pointer[i].info;
    final imageName = basename(dlInfo.dli_fname == nullptr ? 'unknow image' : dlInfo.dli_fname.cast<Utf8>().toDartString());
    final imageAddress = dlInfo.dli_fbase == nullptr ? 0 : dlInfo.dli_fbase.address;
    final symbolName = dlInfo.dli_sname == nullptr ? 'unknow symbol' : dlInfo.dli_sname.cast<Utf8>().toDartString();
    final symbolAddress = dlInfo.dli_saddr == nullptr ? 0 : dlInfo.dli_saddr.address;
    final address = pointer[i].address;

    list.add({
      'imageName': imageName,
      'imageHeaderAddress': imageAddress.toRadixString(16),
      'symbolName': symbolName,
      'symbolAddress': symbolAddress.toRadixString(16),
      'stackAddress': address.toRadixString(16),
      'vmStart': pointer[i].vm_start.toRadixString(16),
      'isolateStart': pointer[i].isolate_start.toRadixString(16),
    });
  }
  return jsonEncode(list);
}


