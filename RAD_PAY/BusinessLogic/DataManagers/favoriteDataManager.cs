using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class favoriteDataManager
    {
        //favoriteViewModel
        //public int            id          { get; set; }
        //public long?          uid         { get; set; }
        //public int?           field_id    { get; set; }
        //public int?           key         { get; set; }
        //public string         value       { get; set; }
        //public string         prefix      { get; set; }
        //public int?           merchant_id { get; set; }
        //public string         name        { get; set; }
        //public int?           fav_id      { get; set; }

        public static void Add(favoriteViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new favorite
            {
                id          = model.id          ,
                uid         = model.uid         ,
                field_id    = model.field_id    ,
                key         = model.key         ,
                value       = model.value       ,
                prefix      = model.prefix      ,
                merchant_id = model.merchant_id ,
                name        = model.name        ,
                fav_id      = model.fav_id,
            };

            db.favorites.Add(dbmodel);
        }

        public static void Modify(favoriteViewModel model, RAD_PAYEntities db)
        {
            var result = db.favorites.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ;
                    dbmodel.uid = model.uid                 ;
                    dbmodel.field_id = model.field_id       ;
                    dbmodel.key = model.key                 ;
                    dbmodel.value = model.value             ;
                    dbmodel.prefix = model.prefix           ;
                    dbmodel.merchant_id = model.merchant_id ;
                    dbmodel.name = model.name               ;
                    dbmodel.fav_id = model.fav_id;
                }
            }
        }

        public static void Delete(favoriteViewModel model, RAD_PAYEntities db)
        {
            var result = db.favorites.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.favorites.Remove(dbmodel);
                }
            }
        }

        public static List<favoriteViewModel> Get(favoriteViewModel model, RAD_PAYEntities db)
        {
            List<favoriteViewModel> list = null;

            var query = from resmodel in db.favorites
                        select new favoriteViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            field_id = resmodel.field_id,
                            key = resmodel.key,
                            value = resmodel.value,
                            prefix = resmodel.prefix,
                            merchant_id = resmodel.merchant_id,
                            name = resmodel.name,
                            fav_id = resmodel.fav_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}