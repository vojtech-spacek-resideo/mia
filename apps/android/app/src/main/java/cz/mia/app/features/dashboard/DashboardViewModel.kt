package cz.mia.app.features.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import cz.mia.app.data.db.TelemetryEntity
import cz.mia.app.data.repositories.EventRepository
import cz.mia.app.data.repository.DeviceRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

@HiltViewModel
class DashboardViewModel @Inject constructor(
	private val repository: EventRepository,
	private val deviceRepository: DeviceRepository
) : ViewModel() {
	val latest = repository.getTelemetry()
		.map { it.firstOrNull() }
		.stateIn(viewModelScope, SharingStarted.Lazily, null as TelemetryEntity?)

	private val _commandResult = MutableStateFlow<CommandResult?>(null)
	val commandResult: StateFlow<CommandResult?> = _commandResult.asStateFlow()

	fun checkDpfStatus() {
		sendAutomotiveCommand("dpf_status")
	}

	fun requestDpfRegeneration() {
		sendAutomotiveCommand("regenerate_dpf")
	}

	fun checkAdBlueLevel() {
		sendAutomotiveCommand("check_additive")
	}

	fun runFullDiagnostics() {
		sendAutomotiveCommand("diagnostics")
	}

	fun readDtcCodes() {
		sendAutomotiveCommand("diagnostics")
	}

	fun clearCommandResult() {
		_commandResult.value = null
	}

	private fun sendAutomotiveCommand(action: String, params: Map<String, Any>? = null) {
		viewModelScope.launch {
			_commandResult.value = CommandResult(action, isLoading = true)
			val result = deviceRepository.sendCommand(
				deviceId = "automotive",
				action = action,
				params = params
			)
			_commandResult.value = when (result) {
				is cz.mia.app.data.repository.Result.Success ->
					CommandResult(action, isLoading = false, message = result.data.message)
				is cz.mia.app.data.repository.Result.Error ->
					CommandResult(action, isLoading = false, error = result.message)
				is cz.mia.app.data.repository.Result.Loading ->
					CommandResult(action, isLoading = true)
			}
		}
	}
}

data class CommandResult(
	val action: String,
	val isLoading: Boolean = false,
	val message: String? = null,
	val error: String? = null
)
