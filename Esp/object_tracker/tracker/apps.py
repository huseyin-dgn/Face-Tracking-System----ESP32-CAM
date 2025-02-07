from django.apps import AppConfig  # AppConfig'i içe aktarın

class TrackerConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'tracker'

    def ready(self):
        import tracker.views  # Uygulama başlatıldığında views modülünü içe aktarın
