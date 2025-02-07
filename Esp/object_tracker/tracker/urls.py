from django.urls import path
from . import views

urlpatterns = [
    path("",views.index,name="index"),
    path("detect-face/", views.detect_face, name="detect_face"),
     path('status/', views.status_view, name='status'),
]