#include "StdAfx.h"
#include "DngDecoderSlices.h"

void *DecodeThread(void *_this) {
  DngDecoderThread* me = (DngDecoderThread*)_this;
  DngDecoderSlices* parent = me->parent;
  try {
    parent->decodeSlice(me);
  } catch (...) {
    pthread_mutex_lock(&parent->errMutex);
    parent->errors.push_back("DNGDEcodeThread: Caught exception.");
    pthread_mutex_unlock(&parent->errMutex);
  }
  pthread_exit(NULL);
  return NULL;
}


DngDecoderSlices::DngDecoderSlices( FileMap* file, RawImage img ) :
mFile(file), mRaw(img) {
  mFixLjpeg = false;
}

DngDecoderSlices::~DngDecoderSlices(void)
{
}

void DngDecoderSlices::addSlice( DngSliceElement slice )
{
  slices.push(slice);
}

void DngDecoderSlices::startDecoding()
{
  // Create threads
#ifdef WIN32
  guint nThreads = pthread_num_processors_np();
#else
  guint nThreads = 2; // FIXME: Port this to unix
#endif
  int slicesPerThread = (slices.size() + nThreads - 1) / nThreads;
//  decodedSlices = 0;
  pthread_attr_t attr;
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_mutex_init(&errMutex, NULL);

  for (guint i = 0; i< nThreads; i++) {
    DngDecoderThread* t = new DngDecoderThread();
    for (int j = 0; j<slicesPerThread ; j++) {
      if (!slices.empty()) {
        t->slices.push(slices.front());
        slices.pop();
      }
    }
    t->parent = this;
    pthread_create(&t->threadid,&attr,DecodeThread,t);
    threads.push_back(t);
  }
  pthread_attr_destroy(&attr);

  void *status;
  for(guint i=0; i<nThreads; i++){
    pthread_join(threads[i]->threadid, &status);
    delete(threads[i]);
  }
  pthread_mutex_destroy(&errMutex);

}

void DngDecoderSlices::decodeSlice( DngDecoderThread* t ) {
  while (!t->slices.empty()) {
    LJpegPlain l(mFile, mRaw);
    l.mDNGCompatible = mFixLjpeg;
    DngSliceElement e = t->slices.front();
    t->slices.pop();
    try {
      l.startDecoder(e.byteOffset, e.byteCount, e.offX, e.offY);
    } catch (RawDecoderException e) { 
      pthread_mutex_lock(&errMutex);
      errors.push_back(_strdup(e.what()));
      pthread_mutex_unlock(&errMutex);
    }
  }
}