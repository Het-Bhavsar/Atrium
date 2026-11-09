// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:atrium/features/device/domain/firmware_update.dart';

/// Pod software from the latest GitHub release of the Atrium repository (`releases/latest`, asset
/// `atrium-pod-<version>.bin`). Only the release is fetched; nothing about the user or the pod is sent. The image is
/// signed, and the pod's bootloader checks the signature before it runs it.
class GitHubReleaseSource implements FirmwareSource {
  GitHubReleaseSource({HttpClient? client, this.endpoint}) : _client = client ?? HttpClient();

  final HttpClient _client;
  final Uri? endpoint;

  static const _timeout = Duration(seconds: 20);

  @override
  Future<FirmwareRelease?> latest() async {
    final req = await _client.getUrl(endpoint ?? Uri.parse(latestReleaseUrl)).timeout(_timeout);
    req.headers
      ..set(HttpHeaders.acceptHeader, 'application/vnd.github+json')
      ..set(HttpHeaders.userAgentHeader, 'Atrium-app');
    final res = await req.close().timeout(_timeout);
    final body = await res.transform(utf8.decoder).join().timeout(_timeout);
    if (res.statusCode == HttpStatus.notFound) return null; // no release yet (or the repository isn't public)
    if (res.statusCode != HttpStatus.ok) throw OtaFailure("Couldn't check for pod software (GitHub answered ${res.statusCode}). Try again later.");
    final json = jsonDecode(body);
    return json is Map<String, dynamic> ? FirmwareRelease.fromGitHub(json) : null;
  }

  @override
  Future<Uint8List> download(FirmwareRelease r, {required void Function(int got, int total) onProgress, required bool Function() isCancelled}) async {
    if (r.size > maxImageBytes) throw const OtaFailure('The published pod software is larger than the pod can take.');
    final req = await _client.getUrl(r.url).timeout(_timeout);
    req.headers.set(HttpHeaders.userAgentHeader, 'Atrium-app');
    final res = await req.close().timeout(_timeout);
    if (res.statusCode != HttpStatus.ok) throw OtaFailure("Couldn't download the pod software (GitHub answered ${res.statusCode}).");
    final total = res.contentLength > 0 ? res.contentLength : r.size;
    final out = BytesBuilder(copy: false);
    await for (final chunk in res.timeout(_timeout)) {
      if (isCancelled()) throw const OtaCancelled();
      out.add(chunk);
      if (out.length > maxImageBytes) throw const OtaFailure('The downloaded file is larger than the pod can take.');
      onProgress(out.length, total);
    }
    if (r.size > 0 && out.length != r.size) throw const OtaFailure('The download was cut short. Try again.');
    return out.takeBytes();
  }
}
