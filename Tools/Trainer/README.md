# Entrenador visible independiente

## Abrir y lanzar varios entrenamientos

En Reinforcement Learning, pulsar **Open Training Window**. También se puede ejecutar out/build/trainer-release/NoJobTrainer.exe directamente. El programa encuentra el proyecto a partir de su ubicación; el editor le pasa la ruta explícitamente.

Cada ventana es un proceso independiente. **Open another training window** abre otra ventana con configuración propia. Elegir nombre, tarea, semilla y parámetros en cada una y pulsar **Start training**. No hay un límite impuesto de ventanas, pero comparten los recursos de CPU del equipo.

Tareas disponibles:

- **Target tracking (X/Z axis policy)**: entrenamiento de una política de un eje, reutilizable en X/Z.
- **Obstacle core (9x9)**: DQN de evasión para la arena fija; por defecto 6000 episodios base y 3000 de refinamiento de casos atascados.

## Qué se ve y cómo se controla

La vista es **2D** y representa el entorno de entrenamiento real: agente azul, objetivo naranja y celdas sólidas rojas. Cambia con cada transición real, incluida la exploración; no reproduce una demostración grabada. Se ven episodio, pasos, recompensa, epsilon, pérdida, acción, intentos bloqueados, observaciones y Q.

- **Visual delay**: 0 ejecuta sin límite artificial; subirlo permite observar las decisiones con calma.
- **Pause / Resume**: pausa o reanuda esa ejecución.
- **Single step**: estando pausado, avanza una transición real.
- **Cancel run**: detiene solo esa ejecución. Conserva su historial, sin guardar una política final incompleta.
- Cerrar una ventana cancela y espera a su propio trabajador. Las demás ventanas y el editor son procesos distintos.

Las gráficas muestran los últimos 2048 episodios. El CSV conserva el historial completo. En modo rápido pueden pasar varios pasos entre dos imágenes; usar pausa, avance por pasos o retardo para inspeccionarlos.

## Archivos y uso de un modelo

Cada ejecución crea una carpeta única en **TrainingRuns/**. Incluso con el mismo nombre, las carpetas no coinciden. Contiene:

- run.txt: tarea, episodios, semilla y parámetros.
- episodes.csv: fase, episodio, recompensa, epsilon, pérdida y pasos; se escribe durante el entrenamiento.
- policy.njn: modelo final, solo cuando el entrenamiento termina.

**Open run folder** abre esa carpeta. Al terminar, **Export to Assets/Neural** copia el modelo con un nombre de asset único y configurable. No reemplaza assets existentes: si el nombre ya existe, elegir otro.

Para probar el modelo exportado, escribir su nombre en PolicyName del agente correspondiente y reiniciar Play. Para el agente compuesto RLObstacleAgent, el modelo de evasión va en PolicyName; una política de seguimiento de un eje va en TrackingPolicyName. La evaluación del panel permite seleccionar directamente el archivo en **Evaluation model** o **Browse...**, tanto desde Assets/Neural como desde TrainingRuns. El tipo de prueba se detecta automáticamente; **Training task** solo configura nuevos entrenamientos. **Refresh models** actualiza la lista al finalizar otro entrenamiento. La pérdida de entrenamiento por sí sola no equivale a éxito en la tarea.

El botón **Train in editor** conserva el entrenamiento anterior. Open Training Window funciona aunque haya un trabajo dentro del editor; las nuevas ventanas no usan su trabajador ni sus resultados.

## Compilación y validación

El proyecto añade el target **NoJobTrainer**, sin enlazar el motor de escenas ni el editor: usa la red nativa y su propio contexto GLFW/ImGui. El lanzador prefiere out/build/trainer-release/NoJobTrainer.exe y admite el ejecutable junto al editor como alternativa.

```sh
cmake --build out/build/x64-Debug --target NoJobTrainer
ctest --test-dir out/build/x64-Debug -R NoJobTrainerSelfTest -V --output-on-failure
```

Se compiló también la versión Release en out/build/trainer-release. Se probaron simultáneamente una ejecución de evasión y otra de seguimiento: directorios/modelos distintos, telemetría real y modelos de dimensiones correctas. También pasaron pausa, un único paso, cancelación y conservación del historial. El auto-test no crea ventanas ni requiere contexto gráfico.

La ventana real se abrió y se observó entrenando, con simulación y gráficas activas. Las acciones de pausa/paso/cancelación se verificaron además en el auto-test. El registro está en TRAINER_TEST_RESULTS.txt.

## Alcance

El visor no muestra toda la escena 3D del editor ni entrena sobre cualquier escena abierta. Entrena los dos entornos nativos descritos. La evasión sigue usando la arena fija; fuera de ella, el agente del juego combina esa política con una política aprendida de seguimiento. No es una única red entrenada sobre todo el escenario ni sobre mapas arbitrarios.

Los trabajos son independientes y no modifican automáticamente el modelo activo del editor. Cambiar la semilla o parámetros permite comparar experimentos; con los mismos parámetros y semilla pueden producir el mismo modelo. No hay una cola central ni gestión remota: cada ventana controla su propia ejecución.
