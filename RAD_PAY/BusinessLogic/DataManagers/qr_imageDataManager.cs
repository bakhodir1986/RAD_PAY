using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class qr_imageDataManager
    {
        //qr_imageViewModel
        //public long       uid      { get; set; }
        //public string     location { get; set; }

        public static void Add(qr_imageViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new qr_image
            {
                uid         = model.uid     ,  
                location    = model.location,
            };

            db.qr_image.Add(dbmodel);
        }

        public static void Modify(qr_imageViewModel model, RAD_PAYEntities db)
        {
            var result = db.qr_image.Where(z => z.uid == model.uid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.uid = model.uid;
                    dbmodel.location = model.location;
                }
            }
        }

        public static void Delete(qr_imageViewModel model, RAD_PAYEntities db)
        {
            var result = db.qr_image.Where(z => z.uid == model.uid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.qr_image.Remove(dbmodel);
                }
            }
        }

        public static List<qr_imageViewModel> Get(qr_imageViewModel model, RAD_PAYEntities db)
        {
            List<qr_imageViewModel> list = null;

            var query = from resmodel in db.qr_image
                        select new qr_imageViewModel
                        {
                            uid = resmodel.uid,
                            location = resmodel.location,
                        };

            list = query.ToList();

            return list;
        }
    }
}