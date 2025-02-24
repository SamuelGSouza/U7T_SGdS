package com.embarcatech.service;

import com.google.cloud.speech.v1.RecognitionAudio;
import com.google.cloud.speech.v1.RecognitionConfig;
import com.google.cloud.speech.v1.RecognizeResponse;
import com.google.cloud.speech.v1.SpeechClient;
import com.google.cloud.speech.v1.SpeechRecognitionResult;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

@Service
public class SpeechToTextService {

    public String transcribeAudio(Path audioPath) throws IOException {
        try (SpeechClient speechClient = SpeechClient.create()) {
            byte[] audioData = Files.readAllBytes(audioPath);

            RecognitionConfig config = RecognitionConfig.newBuilder()
                .setEncoding(RecognitionConfig.AudioEncoding.LINEAR16)
                .setLanguageCode("pt-BR")
                .setSampleRateHertz(16000)
                .build();

            RecognitionAudio audio = RecognitionAudio.newBuilder()
                .setContent(com.google.protobuf.ByteString.copyFrom(audioData))
                .build();

            RecognizeResponse response = speechClient.recognize(config, audio);
            List<SpeechRecognitionResult> results = response.getResultsList();

            StringBuilder transcription = new StringBuilder();
            for (SpeechRecognitionResult result : results) {
                transcription.append(result.getAlternatives(0).getTranscript());
            }

            return transcription.toString();
        }
    }
}
