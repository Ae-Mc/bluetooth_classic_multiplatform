import 'package:flutter/material.dart';

class FutureButton extends StatefulWidget {
  final String text;
  final Future Function() onPressed;

  const FutureButton({super.key, required this.text, required this.onPressed});

  @override
  State<FutureButton> createState() => _FutureButtonState();
}

class _FutureButtonState extends State<FutureButton> {
  Future? _future;

  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
      future: _future,
      builder: (context, snapshot) {
        final widgets = <Widget>[];
        if (snapshot.connectionState == ConnectionState.waiting) {
          widgets.add(CircularProgressIndicator());
        } else {
          if (snapshot.hasError) {
            widgets.add(
              Text(
                'Ошибка: ${snapshot.error}',
                style: TextStyle(color: Colors.red),
              ),
            );
          }
          widgets.add(
            ElevatedButton(
              onPressed: () {
                setState(() {
                  _future = widget.onPressed();
                });
              },
              child: Text(widget.text),
            ),
          );
        }
        return Column(
          mainAxisAlignment: MainAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: widgets,
        );
      },
    );
  }
}
