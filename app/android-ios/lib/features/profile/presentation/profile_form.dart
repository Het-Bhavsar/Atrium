import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Name, age, height and weight. Name is required; the rest is optional and checked for plausible values.
class ProfileForm extends StatefulWidget {
  const ProfileForm({required this.initial, required this.onChanged, super.key});
  final Profile initial;

  /// Called with the edited profile, or null while something is invalid.
  final ValueChanged<Profile?> onChanged;

  @override
  State<ProfileForm> createState() => _ProfileFormState();
}

class _ProfileFormState extends State<ProfileForm> {
  late final _name = TextEditingController(text: widget.initial.name);
  late final _age = TextEditingController(text: widget.initial.age?.toString() ?? '');
  late final _height = TextEditingController(text: widget.initial.heightCm?.toString() ?? '');
  late final _weight = TextEditingController(text: widget.initial.weightKg?.toString() ?? '');

  int? _int(TextEditingController c) => int.tryParse(c.text.trim());

  String? get _ageError => Profile.checkAge(_int(_age));
  String? get _heightError => Profile.checkHeight(_int(_height));
  String? get _weightError => Profile.checkWeight(_int(_weight));

  void _changed() {
    setState(() {});
    final ok = _name.text.trim().isNotEmpty && _ageError == null && _heightError == null && _weightError == null;
    widget.onChanged(ok ? Profile(name: _name.text.trim(), age: _int(_age), heightCm: _int(_height), weightKg: _int(_weight)) : null);
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _changed());
  }

  @override
  void dispose() {
    for (final c in [_name, _age, _height, _weight]) {
      c.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _Field(label: 'Name', controller: _name, hint: 'What should Atrium call you?', onChanged: _changed, words: true),
        const SizedBox(height: Space.m),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: _Field(label: 'Age', controller: _age, suffix: 'years', number: true, error: _ageError, onChanged: _changed),
            ),
            const SizedBox(width: Space.m),
            Expanded(
              child: _Field(label: 'Height', controller: _height, suffix: 'cm', number: true, error: _heightError, onChanged: _changed),
            ),
            const SizedBox(width: Space.m),
            Expanded(
              child: _Field(label: 'Weight', controller: _weight, suffix: 'kg', number: true, error: _weightError, onChanged: _changed),
            ),
          ],
        ),
      ],
    );
  }
}

class _Field extends StatelessWidget {
  const _Field({
    required this.label,
    required this.controller,
    required this.onChanged,
    this.hint,
    this.suffix,
    this.error,
    this.number = false,
    this.words = false,
  });
  final String label;
  final TextEditingController controller;
  final VoidCallback onChanged;
  final String? hint;
  final String? suffix;
  final String? error;
  final bool number;
  final bool words;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final border = OutlineInputBorder(borderRadius: BorderRadius.circular(Radii.small), borderSide: BorderSide.none);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 4, bottom: 6),
          child: Text(label, style: AtriumType.secondary(t.ink2)),
        ),
        TextField(
          controller: controller,
          onChanged: (_) => onChanged(),
          keyboardType: number ? TextInputType.number : TextInputType.name,
          textCapitalization: words ? TextCapitalization.words : TextCapitalization.none,
          inputFormatters: number ? [FilteringTextInputFormatter.digitsOnly, LengthLimitingTextInputFormatter(3)] : [LengthLimitingTextInputFormatter(40)],
          textInputAction: TextInputAction.next,
          autocorrect: false,
          style: AtriumType.body(t.ink),
          cursorColor: t.ink,
          decoration: InputDecoration(
            hintText: hint ?? suffix,
            hintStyle: AtriumType.body(t.ink3),
            suffixText: suffix,
            suffixStyle: AtriumType.secondary(t.ink2),
            errorText: error,
            errorMaxLines: 2,
            errorStyle: AtriumType.caption(t.red),
            filled: true,
            fillColor: t.card,
            isDense: true,
            contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 14),
            border: border,
            enabledBorder: border,
            focusedBorder: border.copyWith(borderSide: BorderSide(color: t.ink2)),
            errorBorder: border.copyWith(borderSide: BorderSide(color: t.red)),
            focusedErrorBorder: border.copyWith(borderSide: BorderSide(color: t.red)),
          ),
        ),
      ],
    );
  }
}
