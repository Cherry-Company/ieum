var FileTransferSessionState_8h =
[
    [ "deskflow::filetransfer::FileTransferSessionMutation", "structdeskflow_1_1filetransfer_1_1FileTransferSessionMutation.html", "structdeskflow_1_1filetransfer_1_1FileTransferSessionMutation" ],
    [ "deskflow::filetransfer::FileTransferDecisionCreation", "structdeskflow_1_1filetransfer_1_1FileTransferDecisionCreation.html", "structdeskflow_1_1filetransfer_1_1FileTransferDecisionCreation" ],
    [ "deskflow::filetransfer::FileTransferSessionRecord", "structdeskflow_1_1filetransfer_1_1FileTransferSessionRecord.html", "structdeskflow_1_1filetransfer_1_1FileTransferSessionRecord" ],
    [ "deskflow::filetransfer::FileTransferSessionRegistry", "classdeskflow_1_1filetransfer_1_1FileTransferSessionRegistry.html", "classdeskflow_1_1filetransfer_1_1FileTransferSessionRegistry" ],
    [ "deskflow::filetransfer::FileTransferSessionError", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521", [
      [ "deskflow::filetransfer::FileTransferSessionError::None", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521a6adf97f83acf6453d4a6a4b1070f3754", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::InvalidMessage", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521ad5e88abe375e264803a6f6b436e769e6", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::DuplicateTransfer", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521ad92e860a328fe3e8f41e3c37b4575888", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::UnknownTransfer", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521a99f093f1665a72734af77557bb0c655b", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::WrongLocalRole", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521a082be668ae9ff0551646fb7c6f1f839f", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::RouteMismatch", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521a167e9a4ca878f0c45d736f3313442537", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::WrongPhase", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521afecceaf033d74e6cf62461783dd6f127", null ],
      [ "deskflow::filetransfer::FileTransferSessionError::CapacityExceeded", "namespacedeskflow_1_1filetransfer.html#af4b1f7c66093d2d87f7f7e26a75e2521afe518b5d77e35a7d9bebb6025ba8f992", null ]
    ] ],
    [ "deskflow::filetransfer::FileTransferSessionPhase", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bc", [
      [ "deskflow::filetransfer::FileTransferSessionPhase::AwaitingDecision", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bcad6a913d7d6bcd35d01b7b1d5d9981d2c", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::ReadyToSend", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bca45ac3bfc16b1b6e85bb12de4fb36852b", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::AwaitingData", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bca6c1106b6985d056b946311b8eb6e2c55", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Sending", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bcae4b0c2b6d59cb4cf3e169a9886008087", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Receiving", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bca338f550ea5a0bcb9ac785f48b27f4f12", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Rejected", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bcad37b1f6c0512e2118cee17fea015b699", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Completed", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bca07ca5050e697392c9ed47e6453f1453f", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Failed", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bcad7c8c85bf79bbe1b7188497c32c3b0ca", null ],
      [ "deskflow::filetransfer::FileTransferSessionPhase::Cancelled", "namespacedeskflow_1_1filetransfer.html#afd36a6ac98e777cd6243bd7faef173bcaa149e85a44aeec9140e92733d9ed694e", null ]
    ] ],
    [ "deskflow::filetransfer::FileTransferSessionRole", "namespacedeskflow_1_1filetransfer.html#a5dee6feb18c08b58f5bb722fb7f96734", [
      [ "deskflow::filetransfer::FileTransferSessionRole::Sender", "namespacedeskflow_1_1filetransfer.html#a5dee6feb18c08b58f5bb722fb7f96734a8aace3ec18d83874d22850b7eee93c7d", null ],
      [ "deskflow::filetransfer::FileTransferSessionRole::Receiver", "namespacedeskflow_1_1filetransfer.html#a5dee6feb18c08b58f5bb722fb7f96734aa9d093d11bc6e98b0c8e586ffa545c85", null ]
    ] ]
];