
- source_spec: `spec-plant-robot-dashboard-local.md`
  summary: Mesurer le coût des recherches d’ancrages sur un journal plein avec plus de64boots retenus.
  evidence: R2-B6, medium non vérifié ; les lectures synchrones sont présentes mais le dépassement des délais HTTP exige un benchmark ESP32 avec distribution de données et latence flash réalistes.
- source_spec: `spec-plant-robot-dashboard-local.md`
  summary: Vérifier le portail lorsque AP et routeur domestique partagent 192.168.4.0/24.
  evidence: R2-B8, medium non vérifié ; le SDK active le routage IPv4 selon la source. Essai réel de DHCP/adresses identiques/réponses TCP nécessaire pour conclure et choisir une migration de sous-réseau.
