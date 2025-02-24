package com.embarcatech.service;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.boot.context.properties.EnableConfigurationProperties;

@ConfigurationProperties(prefix = "audio")
@EnableConfigurationProperties(AudioStorageProperties.class)
public record AudioStorageProperties(String uploadDir) {
}