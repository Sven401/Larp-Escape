const { resultToViewState, statusToViewState } = require("./presentation");

function buildGameMasterScreen(status, lastRequest, lastResult, options = {}) {
  const view = statusToViewState(status, options);
  const enabledActions = view.actions.filter((action) => action.enabled);
  const disabledActions = view.actions.filter((action) => !action.enabled);
  return {
    screenTitle: "Spielleitung",
    connection: {
      state: view.connection,
      primary: view.connection === "mock-connected" ? "Mock-Geraet verbunden" : "Geraet nicht verbunden",
      detail: `Boot ${view.technical.bootId}`,
    },
    profile: {
      primary: view.profileName,
      detail: `Profilrevision ${view.profileRevision}, Speicher ${view.storageGeneration}`,
      warning: view.storedMatchesActive ? null : "Aktives Profil weicht vom gespeicherten Stand ab.",
    },
    runtime: {
      primary: view.title,
      detail: `${view.mode} / ${view.operation}`,
      revision: `Runtime ${view.technical.runtimeGeneration}, Konfiguration ${view.technical.configRevision}`,
    },
    actions: enabledActions.map((action) => ({
      commandType: action.commandType,
      label: action.label,
      style: action.commandType === "reset" ? "caution" : "primary",
    })),
    unavailableActions: disabledActions.map((action) => ({
      commandType: action.commandType,
      label: action.label,
    })),
    feedback: lastRequest && lastResult ? resultToViewState(lastRequest, lastResult) : null,
    operation: view.operationState,
    fault: view.lastError,
  };
}

function renderGameMasterScreenText(screen) {
  const lines = [
    screen.screenTitle,
    `${screen.connection.primary} (${screen.connection.detail})`,
    `Profil: ${screen.profile.primary} - ${screen.profile.detail}`,
    `Spiel: ${screen.runtime.primary} - ${screen.runtime.detail}`,
    screen.runtime.revision,
  ];

  if (screen.profile.warning) {
    lines.push(`Hinweis: ${screen.profile.warning}`);
  }

  lines.push(`Aktionen: ${screen.actions.map((action) => action.label).join(", ") || "keine"}`);

  if (screen.feedback) {
    lines.push(`${screen.feedback.title}: ${screen.feedback.message}`);
  }

  if (screen.fault) {
    lines.push(`Fehler: ${screen.fault}`);
  }

  return lines.join("\n");
}

module.exports = {
  buildGameMasterScreen,
  renderGameMasterScreenText,
};
