import 'package:flutter_test/flutter_test.dart';

import 'package:mobile/main.dart';
import 'package:mobile/services/simulator_service.dart';

void main() {
  testWidgets('shows the four main tabs', (WidgetTester tester) async {
    final transport = SimulatorService();
    await tester.pumpWidget(AutonomousVehicleApp(transport: transport));

    expect(find.text('Map'), findsWidgets);
    expect(find.text('Test'), findsOneWidget);
    expect(find.text('Control'), findsOneWidget);
    expect(find.text('Wheels'), findsOneWidget);

    transport.dispose();
  });
}
